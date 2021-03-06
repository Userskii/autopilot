/**************************************************************************
 * Copyright 2012 Bryan Godbolt
 * 
 * This file is part of ANCL Autopilot.
 * 
 *     ANCL Autopilot is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 * 
 *     ANCL Autopilot is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with ANCL Autopilot.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "Control.h"

/* Project Headers */
#include "servo_switch.h"
#include "bad_control.h"
#include "RCTrans.h"
#include "IMU.h"
#include "QGCLink.h"

/* Boost Headers */
#include <boost/bind.hpp>

Control::Control()
:pilot_mix(6,1), // fill pilot_mix with 1s
 config_file_buffer(NULL),
 controller_mode(heli::Num_Controller_Modes),
 mode_connection(QGCLink::getInstance()->control_mode.connect(
		 boost::bind(&Control::set_controller_mode, this, _1))),
 reference_position(3)
{
	// load config file
	loadFile();
	reference_position.clear();
}

Control* Control::_instance = NULL;
boost::mutex Control::_instance_lock;

Control* Control::getInstance()
{
	boost::mutex::scoped_lock lock(_instance_lock);
	if (!_instance)
		_instance = new Control();
	return _instance;
}

/* Set the parameter name strings */
const std::string Control::PARAM_MIX_ROLL = "MIX_ROLL";
const std::string Control::PARAM_MIX_PITCH = "MIX_PITCH";
const std::string Control::CONTROL_MODE = "MODE_CONTROL";

std::vector<Parameter> Control::getParameters()
{
	// create vector to collect parameters from all controllers
	std::vector<Parameter> plist;

	// push pilot mix params
	plist.push_back(Parameter(PARAM_MIX_ROLL, pilot_mix[ROLL], heli::CONTROLLER_ID));
	plist.push_back(Parameter(PARAM_MIX_PITCH, pilot_mix[PITCH], heli::CONTROLLER_ID));
//	plist.push_back(Parameter(CONTROL_MODE, get_controller_mode(), heli::CONTROLLER_ID));

	// append parameters from pid controller
	std::vector<Parameter> controller_params(attitude_pid_controller().getParameters());
	plist.insert(plist.begin() + plist.size(), controller_params.begin(), controller_params.end());

	std::vector<Parameter> translation_controller_params(translation_pid_controller().getParameters());
	plist.insert(plist.end(), translation_controller_params.begin(), translation_controller_params.end());

	// append parameters from any other controllers here

	// return the complete parameter list
	return plist;
}

void Control::setParameter(Parameter p)
{
	std::string param_id(p.getParamID());
	boost::trim(param_id);
	if (param_id == attitude_pid::PARAM_ROLL_KP)
		attitude_pid_controller().set_roll_proportional(p.getValue());
	else if (param_id == attitude_pid::PARAM_ROLL_KD)
		attitude_pid_controller().set_roll_derivative(p.getValue());
	else if (param_id == attitude_pid::PARAM_ROLL_KI)
		attitude_pid_controller().set_roll_integral(p.getValue());
	else if (param_id == attitude_pid::PARAM_PITCH_KP)
		attitude_pid_controller().set_pitch_proportional(p.getValue());
	else if (param_id == attitude_pid::PARAM_PITCH_KD)
		attitude_pid_controller().set_pitch_derivative(p.getValue());
	else if (param_id == attitude_pid::PARAM_PITCH_KI)
		attitude_pid_controller().set_pitch_integral(p.getValue());

	else if (param_id == PARAM_MIX_ROLL)
		set_roll_mix(p.getValue());
	else if (param_id == PARAM_MIX_PITCH)
		set_pitch_mix(p.getValue());
//	else if (param_id == CONTROL_MODE)
//		set_controller_mode(static_cast<heli::Controller_Mode>(p.getValue()));
	else if (param_id == attitude_pid::PARAM_ROLL_TRIM)
		attitude_pid_controller().set_roll_trim_degrees(p.getValue());
	else if (param_id == attitude_pid::PARAM_PITCH_TRIM)
		attitude_pid_controller().set_pitch_trim_degrees(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_X_KP)
		translation_pid_controller().set_x_proportional(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_X_KD)
		translation_pid_controller().set_x_derivative(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_X_KI)
		translation_pid_controller().set_x_integral(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_Y_KP)
		translation_pid_controller().set_y_proportional(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_Y_KD)
		translation_pid_controller().set_y_derivative(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_Y_KI)
		translation_pid_controller().set_y_integral(p.getValue());
	else if (param_id == translation_outer_pid::PARAM_TRAVEL)
		translation_pid_controller().set_scaled_travel_degrees(p.getValue());
	else
	{
		warning() << "Control::setParameter - unknown parameter: " << p;
		return;
	}
	saveFile();
}

blas::vector<double> Control::get_control_effort() const
{
	std::vector<double> pilot_inputs(RCTrans::getScaledVector());

	// compute control effort
	blas::vector<double> control_effort(attitude_pid_controller().get_control_effort());
	control_effort.resize(6);

	if (!(pilot_inputs.size() == control_effort.size() && pilot_inputs.size() == 6 && pilot_inputs.size() == pilot_mix.size()))
	{
		bad_control b("At least one of the vectors are not of length 6", boost::lexical_cast<std::string>(__FILE__), __LINE__);
		throw b;
	}

	blas::vector<double> control_output(6);

	// mix according to pilot_mix
	for (unsigned int i=0; i < control_output.size(); i++)
	{
		if (!(pilot_mix[i] >= 0 && pilot_mix[i] <= 1))
		{
			bad_control b(" Pilot mix values is out of range.", boost::lexical_cast<std::string>(__FILE__), __LINE__);
			throw b;
		}
		control_output[i] = pilot_mix[i]*pilot_inputs[i] + (1-pilot_mix[i])*control_effort[i];
	}

	// log control effort
	LogFile::getInstance()->logData("Control Effort", control_effort);
	//log final control output
	LogFile::getInstance()->logData("Mixed Control Output", control_output);
	return control_output;
}

void Control::set_roll_mix(double roll_mix)
{
	if (roll_mix <= 1 && roll_mix >= 0)
	{
		{
			boost::mutex::scoped_lock lock(pilot_mix_lock);
			pilot_mix[ROLL] = roll_mix;
		}
		message() << "Changed roll pilot mix to: " << roll_mix;
	}
	else
		message() << "Invalid roll mix argument: " << roll_mix;
}

void Control::set_pitch_mix(double pitch_mix)
{

	if (pitch_mix <= 1 && pitch_mix >= 0)
	{
		{
			boost::mutex::scoped_lock lock(pilot_mix_lock);
			pilot_mix[PITCH] = pitch_mix;
		}
		message() << "Changed pitch pilot mix to: " << pitch_mix;
	}

	else
		message() << "Invalid pitch mix argument: " << pitch_mix;
}

void Control::loadFile()
{
	if (!boost::filesystem::exists(heli::calibration_filename))
	{
		warning() << __FILE__ << __LINE__ << "Cannot find controller parameter xml file: " << heli::controller_param_filename;
		return;
	}
	/* Read contents of configuration file into char* */
	std::ifstream config_file;
	int length;
	config_file_lock.lock();
	config_file.open(heli::controller_param_filename.c_str());      // open input file
	config_file.seekg(0, std::ios::end);    // go to the end
	length = config_file.tellg();           // report location (this is the length)
	config_file.seekg(0, std::ios::beg);    // go back to the beginning
	if (config_file_buffer)
			delete config_file_buffer;
	config_file_buffer = new char[length+1];    // allocate memory for a buffer of appropriate dimension
	config_file.read(config_file_buffer, length);       // read the whole file into the buffer
	config_file.close();
	config_file_lock.unlock();
	config_file_buffer[length] = 0;

	config_file_xml.parse<0>(config_file_buffer);

	rapidxml::xml_node<> *root_node = config_file_xml.first_node();

	if (boost::to_upper_copy(std::string(root_node->name())) != "CONTROLLER_PARAMS")
	{
		critical() << "Control::loadFile() Unknown file format.  Cannot load controller parameters.";
		return;
	}

	for (rapidxml::xml_node<> *node = root_node->first_node(); node; node = node->next_sibling())
	{
		std::string node_name(node->name());
		if (boost::to_upper_copy(std::string(node_name)) == "MIX")
			parse_pilot_mix(node);
		else if (boost::to_upper_copy(std::string(node_name)) == "MODE")
			parse_mode(node);
		else if (std::string(node_name) == "attitude_pid")
			attitude_pid_controller().parse_pid(node);
		else if (std::string(node_name) == "translation_outer_pid")
			translation_pid_controller().parse_xml_node(node);
		else
			warning() << __FILE__ << __LINE__ << "Found unknown node: " << node_name;
	}
}

void Control::parse_pilot_mix(rapidxml::xml_node<> *mix)
{

	std::string mix_value(mix->value());
	boost::trim(mix_value);

	// find which channel to set
	rapidxml::xml_attribute<> *attr;
	for (attr = mix->first_attribute(); attr && std::string(attr->name()) != "channel"; attr = attr->next_attribute());
	std::string channel(attr->value());
	boost::to_upper(channel);
	if (channel == "ROLL")
		set_roll_mix(boost::lexical_cast<double>(mix_value));
	else if (channel == "PITCH")
		set_pitch_mix(boost::lexical_cast<double>(mix_value));
}

void Control::parse_mode(rapidxml::xml_node<> *mode)
{
	std::string mode_value(mode->value());
	boost::trim(mode_value);
	set_controller_mode(static_cast<heli::Controller_Mode>(boost::lexical_cast<unsigned int>(mode_value)));
}

void Control::operator()()
{

	if (get_controller_mode() == heli::Mode_Position_Hold_PID)
	{
		if (translation_pid_controller().runnable())
		{
			try
			{
				translation_pid_controller()(get_reference_position());
				blas::vector<double> roll_pitch_reference(translation_pid_controller().get_control_effort());
				LogFile::getInstance()->logData(heli::LOG_TRANS_ATTITUDE_REF, roll_pitch_reference);
				attitude_pid_controller()(roll_pitch_reference);
			}
			catch (bad_control& bc)
			{
				warning() << "Caught exception from Translational PID, switching to attitude stabilization mode";
				set_controller_mode(heli::Mode_Attitude_Stabilization_PID);
			}
		}
		else
		{
			warning() <<"Control: translation controller reports it is not runnable.  Switching to attitude control.";
			set_controller_mode(heli::Mode_Attitude_Stabilization_PID);
		}

		// prevents exception being thrown
		return;
	}
	// not else if so that it will run if the mode was changed
	if (get_controller_mode() == heli::Mode_Attitude_Stabilization_PID)
	{
		blas::vector<double> roll_pitch_reference(2);
		roll_pitch_reference[ROLL] = attitude_pid_controller().get_roll_trim_radians();
		roll_pitch_reference[PITCH] = attitude_pid_controller().get_pitch_trim_radians();
		attitude_pid_controller()(roll_pitch_reference);

		// prevents exception being thrown
		return;
	}
	throw bad_control("Control: not set to valid control mode");
}

void Control::saveFile()
{
	rapidxml::xml_document<> config_file_xml;
	rapidxml::xml_node<> *root_node = config_file_xml.allocate_node(rapidxml::node_element, "controller_params");
	config_file_xml.append_node(root_node);

	/* get pid params */
	rapidxml::xml_node<> *pid_node = attitude_pid_controller().get_xml_node(config_file_xml);
	root_node->append_node(pid_node);

	/* get trans pid params */
	rapidxml::xml_node<> *trans_pid_node = translation_pid_controller().get_xml_node(config_file_xml);
	root_node->append_node(trans_pid_node);

	/* add pilot mixes */
	rapidxml::xml_node<> *node = NULL;
	char *node_value = NULL;
	rapidxml::xml_attribute<> *attr = NULL;

	// roll mix
	pilot_mix_lock.lock();
	node_value = config_file_xml.allocate_string(boost::lexical_cast<std::string>(pilot_mix[ROLL]).c_str());
	pilot_mix_lock.unlock();
	node = config_file_xml.allocate_node(rapidxml::node_element, "mix", node_value);
	attr = config_file_xml.allocate_attribute("channel", "roll");
	node->append_attribute(attr);
	root_node->append_node(node);

	// pitch mix
	pilot_mix_lock.lock();
	node_value = config_file_xml.allocate_string(boost::lexical_cast<std::string>(pilot_mix[PITCH]).c_str());
	pilot_mix_lock.unlock();
	node = config_file_xml.allocate_node(rapidxml::node_element, "mix", node_value);
	attr = config_file_xml.allocate_attribute("channel", "pitch");
	node->append_attribute(attr);
	root_node->append_node(node);

	node_value = config_file_xml.allocate_string(boost::lexical_cast<std::string>(get_controller_mode()).c_str());
	node = config_file_xml.allocate_node(rapidxml::node_element, "mode", node_value);
	root_node->append_node(node);

	std::ofstream config_file;
	config_file_lock.lock();
	config_file.open(heli::controller_param_filename.c_str());
	config_file << config_file_xml;
	config_file.close();
	config_file_lock.unlock();
}

std::string Control::getModeString(heli::Controller_Mode mode)
{
	if (mode == heli::Mode_Attitude_Stabilization_PID)
		return "MODE_ATTITUDE_STABILIZATION_PID";
	else if (mode == heli::Mode_Position_Hold_PID)
		return "MODE_POSITION_HOLD_PID";
	return std::string();
}

void Control::set_reference_position()
{
	blas::vector<double> reference(IMU::getInstance()->get_ned_position());
	set_reference_position(reference);
	message() << "Control: Position reference set to: " << reference;
}
void Control::set_controller_mode(heli::Controller_Mode mode)
{
	bool mode_changed = false;
	if (mode < heli::Num_Controller_Modes)
	{
		boost::mutex::scoped_lock lock(controller_mode_lock);
		if (controller_mode != mode)
			mode_changed = true;
		controller_mode = mode;
	}
	if (mode_changed)
	{
		this->mode_changed(mode);
		warning() << "Controller mode changed to: " << getModeString(mode);
	}
}

void Control::reset()
{
	x_y_pid_controller.reset();
	roll_pitch_pid_controller.reset();
}

bool Control::runnable() const
{
	// control is runnable as long as attitude can still be controlled
	return attitude_pid_controller().runnable();
}

