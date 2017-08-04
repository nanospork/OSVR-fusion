#include "stdafx.h"
#include <iostream>

namespace je_nourish_fusion {

	IOrientationReader* OrientationReaderFactory::getReader(OSVR_ClientContext ctx, Json::Value config) {
		IOrientationReader* reader = NULL;

		if (config.isString()) {
			reader = new SingleOrientationReader(ctx, config.asString());
		}
		else if (config.isObject() && config.isMember("roll") && config.isMember("pitch") && config.isMember("yawFast")
			&& config.isMember("yawAccurate") && config.isMember("alpha")) {
			reader = new FilteredOrientationReader(ctx, config);
		}
		else if (config.isObject() && config.isMember("roll") && config.isMember("pitch") && config.isMember("yaw")) {
			reader = new CombinedOrientationReader(ctx, config);
		}

		return reader;
	}

	SingleOrientationReader::SingleOrientationReader(OSVR_ClientContext ctx, std::string orientation_path) {
		osvrClientGetInterface(ctx, orientation_path.c_str(), &m_orientation);
	}
	OSVR_ReturnCode SingleOrientationReader::update(OSVR_OrientationState* orientation, OSVR_TimeValue* timeValue) {
		return osvrGetOrientationState(m_orientation, timeValue, orientation);
	}

	CombinedOrientationReader::CombinedOrientationReader(OSVR_ClientContext ctx, Json::Value orientation_paths) {
		osvrClientGetInterface(ctx, orientation_paths["roll"].asCString(), &(m_orientations[0]));
		osvrClientGetInterface(ctx, orientation_paths["pitch"].asCString(), &(m_orientations[1]));
		osvrClientGetInterface(ctx, orientation_paths["yaw"].asCString(), &(m_orientations[2]));
	}

	FilteredOrientationReader::FilteredOrientationReader(OSVR_ClientContext ctx, Json::Value orientation_paths) {
		osvrClientGetInterface(ctx, orientation_paths["roll"].asCString(), &(m_orientations[0]));
		osvrClientGetInterface(ctx, orientation_paths["pitch"].asCString(), &(m_orientations[1]));
		osvrClientGetInterface(ctx, orientation_paths["yawFast"].asCString(), &(m_orientations[2]));
		osvrClientGetInterface(ctx, orientation_paths["yawAccurate"].asCString(), &(m_orientations[3]));
		// TO DO: Is this a bogus way to read in this value? Do I care?
		m_alpha = orientation_paths["alpha"].asDouble();
	}

	OSVR_ReturnCode CombinedOrientationReader::update(OSVR_OrientationState* orientation, OSVR_TimeValue* timeValue) {
		OSVR_OrientationState orientation_x;
		OSVR_OrientationState orientation_y;
		OSVR_OrientationState orientation_z;

		OSVR_ReturnCode xret = osvrGetOrientationState(m_orientations[0], timeValue, &orientation_x);
		OSVR_ReturnCode yret = osvrGetOrientationState(m_orientations[1], timeValue, &orientation_y);
		OSVR_ReturnCode zret = osvrGetOrientationState(m_orientations[2], timeValue, &orientation_z);

		OSVR_Vec3 rpy_x;
		OSVR_Vec3 rpy_y;
		OSVR_Vec3 rpy_z;

		rpyFromQuaternion(&orientation_x, &rpy_x);
		rpyFromQuaternion(&orientation_y, &rpy_y);
		rpyFromQuaternion(&orientation_z, &rpy_z);

		OSVR_Vec3 rpy;
		osvrVec3SetX(&rpy, osvrVec3GetX(&rpy_x));
		osvrVec3SetY(&rpy, osvrVec3GetY(&rpy_y));
		osvrVec3SetZ(&rpy, osvrVec3GetZ(&rpy_z));

		quaternionFromRPY(&rpy, orientation);

		return OSVR_RETURN_SUCCESS;
	}

	OSVR_ReturnCode FilteredOrientationReader::update(OSVR_OrientationState* orientation, OSVR_TimeValue* timeValue) {
		OSVR_OrientationState orientation_x;
		OSVR_OrientationState orientation_y;
		OSVR_OrientationState orientation_z;
		OSVR_OrientationState orientation_z2;

		OSVR_ReturnCode xret = osvrGetOrientationState(m_orientations[0], timeValue, &orientation_x);
		OSVR_ReturnCode yret = osvrGetOrientationState(m_orientations[1], timeValue, &orientation_y);
		OSVR_ReturnCode zret = osvrGetOrientationState(m_orientations[2], timeValue, &orientation_z);
		OSVR_ReturnCode z2ret = osvrGetOrientationState(m_orientations[3], timeValue, &orientation_z2);

		OSVR_Vec3 rpy_x;
		OSVR_Vec3 rpy_y;
		OSVR_Vec3 rpy_z;
		OSVR_Vec3 rpy_z2;

		rpyFromQuaternion(&orientation_x, &rpy_x);
		rpyFromQuaternion(&orientation_y, &rpy_y);
		rpyFromQuaternion(&orientation_z, &rpy_z);
		rpyFromQuaternion(&orientation_z2, &rpy_z2);

		/*---------------------------------------
		// Filter Implementation Here
		---------------------------------------*/
		double z_fast;
		double z_accurate;
		double z_out;
		double a = m_alpha;
		double last_y = m_last_yaw;
		double dt = osvr::util::time::duration(*timeValue, *m_last_timeValue);

		z_fast = osvrVec3GetZ(&rpy_z);
		z_accurate = osvrVec3GetZ(&rpy_z2);

		//z_fast = fixUnityAngle(z_fast);
		//z_accurate = fixUnityAngle(z_accurate);

		z_out = a*(last_y + z_fast*dt) + (1 - a)*(z_accurate);

		z_out = fixUnityAngle(z_out);

		m_last_yaw = z_out;
		m_last_timeValue = timeValue;

		OSVR_Vec3 rpy;
		osvrVec3SetX(&rpy, osvrVec3GetX(&rpy_x));
		osvrVec3SetY(&rpy, osvrVec3GetY(&rpy_y));
		osvrVec3SetZ(&rpy, z_out);

		quaternionFromRPY(&rpy, orientation);

		return OSVR_RETURN_SUCCESS;
	}

}