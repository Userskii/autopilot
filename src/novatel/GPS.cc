/*******************************************************************************
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
 ******************************************************************************/

#include "GPS.h"

/* Project Headers */
#include "novatel_read_serial.h"

/* C Headers */
#include <math.h>

GPS* GPS::_instance = 0;
boost::mutex GPS::_instance_lock;

GPS* GPS::getInstance()
{
	boost::mutex::scoped_lock lock(_instance_lock);
	if (!_instance)
		_instance = new GPS();
	return _instance;
}

GPS::GPS()
: read_serial_thread(read_serial())
//  mode(heli::MODE_GPS_UNINITIALIZED),
//  _pos_count(0),
//  _vel_count(0),
//  _terminate(false)
{
//	LogFile::getInstance()->logHeader(heli::LOG_NOVATEL_GPS, "P-sol_status pos_type P-X P-Y P-Z P-X_stddev P-Y_stddev P-Z_stddev "
//			"V-sol_status vel_type V-X V-Y V-Z V-X_stddev V-Y_stddev V-Z_stddev "
//			"V-latency diff_age sol_age #obs #GPSL1 #L1 #L2");

//	MainApp::add_thread(&read_serial_thread, "Novatel GPS");
//	MainApp::terminate.connect(GPS::terminate());
}

GPS::~GPS()
{

}

/* GPS functions */



