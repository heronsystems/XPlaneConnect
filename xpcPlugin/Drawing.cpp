// Copyright (c) 2013-2018 United States Government as represented by the Administrator of the
// National Aeronautics and Space Administration. All Rights Reserved.
//
// X-Plane API
// Copyright(c) 2008, Sandy Barbour and Ben Supnik All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files(the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Neither the names of the authors nor that of X - Plane or Laminar Research
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission from the authors or
//     Laminar Research, respectively.
#define _USE_MATH_DEFINES

#define XPLM302

#include "Drawing.h"

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"

#include <cmath>
#include <string>
#include <cstring>
#include <array>
// OpenGL includes

#define GL_GLEXT_PROTOTYPES
#if IBM
#  include <windows.h>
#endif

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include <chrono>
#include <cstdint>

#include <Eigen/Dense>
#include <Eigen/Geometry> 


namespace XPC
{
	// Internal Structures
	typedef struct
	{
		double x;
		double y;
		double z;
	} LocalPoint;

	// Internal Memory
	static const size_t MSG_MAX = 1024;
	static const size_t MSG_LINE_MAX = MSG_MAX / 16;
	static bool msgEnabled = false;
	static int msgX = -1;
	static int msgY = -1;
	static char msgVal[MSG_MAX] = { 0 };
	static size_t newLineCount = 0;
	static size_t newLines[MSG_LINE_MAX] = { 0 };
	static float rgb[3] = { 0.25F, 1.0F, 0.25F };
	static float rgb_red[3] = { 1.0F, 0.0F, 0.0F };

	static const size_t WAYPOINT_MAX = 128;
	static bool routeEnabled = false;
	static size_t numWaypoints = 0;
	static Waypoint waypoints[WAYPOINT_MAX];
	static LocalPoint localPoints[WAYPOINT_MAX];

	XPLMDataRef ref_blueX;
	XPLMDataRef	ref_blueY;
	XPLMDataRef	ref_blueZ;

	XPLMDataRef ref_bluePitch_deg;
	XPLMDataRef ref_blueHeading_deg;
	XPLMDataRef ref_blueRoll_deg;

	XPLMDataRef ref_redX;
	XPLMDataRef ref_redY;
	XPLMDataRef ref_redZ;

	XPLMDataRef ref_redPitch_deg;
	XPLMDataRef ref_redHeading_deg;
	XPLMDataRef ref_redRoll_deg;

	XPLMDataRef ref_cam_pitch;
	XPLMDataRef ref_cam_vert_FOV_deg;


	GLdouble model_view[16];		
	GLdouble projection[16];	
	GLint viewport[4];

	GLdouble pos3D_x, pos3D_y, pos3D_z;
	float prevDistance_meter = 0;
	uint64_t prevDistance_time = 0;
	bool closureRateCalculated = false;
	float closureCalculation_m_sec = 0;


	float target_prev_pos_x;
	float target_prev_pos_y;
	float target_prev_pos_z;
	uint64_t target_prev_time = 0;
	bool targetSpeedCalculated = false;
	float targetSpeed_m_sec = 0;


	float fps_to_knot = 0.592484;
	float meter_to_ft = 3.28084;
	float ft_to_meter = 1 / meter_to_ft;

	float conversion_speed = meter_to_ft * fps_to_knot;
	float conversion_distance = meter_to_ft;

	float score_WEZ_angle_deg = 1;
	float score_WEZ_min_ft = 500;
	float score_WEZ_max_ft = 3000;

	// Internal Functions

	/// Comparse two size_t integers. Used by qsort in RemoveWaypoints.
	static int cmp(const void * a, const void * b)
	{
		std::size_t sa = *(size_t*)a;
		std::size_t sb = *(size_t*)b;
		if (sa > sb)
		{
			return 1;
		}
		if (sb > sa)
		{
			return -1;
		}
		return 0;
	}


	static float makerSize(float d) {
		const float TAN = 0.00436335F;
		float h = d * TAN;
		return h;
	}

	/// Draws a cube centered at the specified OpenGL world coordinates.
	///
	/// \param x The X coordinate.
	/// \param y The Y coordinate.
	/// \param z The Z coordinate.
	/// \param d The distance from the player airplane to the center of the cube.
	static void gl_drawCube(float x, float y, float z, float d)
	{
		float h = makerSize(d);

		glBegin(GL_QUAD_STRIP);
		// Top
		glVertex3f(x - h, y + h, z - h);
		glVertex3f(x + h, y + h, z - h);
		glVertex3f(x - h, y + h, z + h);
		glVertex3f(x + h, y + h, z + h);
		// Front
		glVertex3f(x - h, y - h, z + h);
		glVertex3f(x + h, y - h, z + h);
		// Bottom
		glVertex3f(x - h, y - h, z - h);
		glVertex3f(x + h, y - h, z - h);
		// Back
		glVertex3f(x - h, y + h, z - h);
		glVertex3f(x + h, y + h, z - h);

		glEnd();
		glBegin(GL_QUADS);
		// Left
		glVertex3f(x - h, y + h, z - h);
		glVertex3f(x - h, y + h, z + h);
		glVertex3f(x - h, y - h, z + h);
		glVertex3f(x - h, y - h, z - h);
		// Right
		glVertex3f(x + h, y + h, z + h);
		glVertex3f(x + h, y + h, z - h);
		glVertex3f(x + h, y - h, z - h);
		glVertex3f(x + h, y - h, z + h);

		glEnd();
	}

	static void gl_drawRectangle(float x, float y, float z, float w, float h, float d)
	{

		glBegin(GL_QUAD_STRIP);
		// Top
		glVertex3f(x - w, y + h, z - d);
		glVertex3f(x + w, y + h, z - d);
		glVertex3f(x - w, y + h, z + d);
		glVertex3f(x + w, y + h, z + d);
		// Front
		glVertex3f(x - w, y - h, z + d);
		glVertex3f(x + w, y - h, z + d);
		// Bottom
		glVertex3f(x - w, y - h, z - d);
		glVertex3f(x + w, y - h, z - d);
		// Back
		glVertex3f(x - w, y + h, z - d);
		glVertex3f(x + w, y + h, z - d);

		glEnd();
		glBegin(GL_QUADS);
		// Left
		glVertex3f(x - w, y + h, z - d);
		glVertex3f(x - w, y + h, z + d);
		glVertex3f(x - w, y - h, z + d);
		glVertex3f(x - w, y - h, z - d);
		// Right
		glVertex3f(x + w, y + h, z + d);
		glVertex3f(x + w, y + h, z - d);
		glVertex3f(x + w, y - h, z - d);
		glVertex3f(x + w, y - h, z + d);

		glEnd();
	}

	static void gl_drawSphere(float x, float y, float z, float r)
	{
		int slices = 100;
		int stacks = 100;

		glPushMatrix();
		// Draw sphere (possible styles: FILL, LINE, POINT).
		//gl.glColor3f(0.3f, 0.5f, 1f);
		glTranslated(x, y, z);
		GLUquadric *sphere = gluNewQuadric();
		gluSphere(sphere, r, slices, stacks);
		gluDeleteQuadric(sphere);
		glPopMatrix();

		
	}


	//a : vector of heading and pitch angles in order.  Heading taken in degrees, pitch in rad
	//    returns a 3x3 rotation matrix for Heading, Pitch.  Assume vector aligned with roll axis
	static Eigen::Matrix3f rot_mat_HP(float a0, float a1) {
		float neg_a_rad = -a0 * M_PI / 180.0;
		float cos_a1 = cosf(a1);
		float sin_a1 = sinf(a1);
		float cos_a_rad = cosf(neg_a_rad);
		float sin_a_rad = sinf(neg_a_rad);
		Eigen::Matrix3f rot_Head;
		rot_Head << cos_a_rad, -sin_a_rad, 0,
			sin_a_rad, cos_a_rad, 0,
			0, 0, 1;
		Eigen::Matrix3f rot_Pitch;
		rot_Pitch << 1, 0, 0,
			0, cos_a1, -sin_a1,
			0, sin_a1, cos_a1;

		Eigen::Matrix3f mat = rot_Head * rot_Pitch;
		return mat;
	};

	/// Draws the string set by the TEXT command.
	static int MessageDrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void * inRefcon)
	{

		// const char* msg = (std::to_string(msgX) + " " + std::to_string(msgY)).c_str();
		// size_t len = strnlen(msg, MSG_MAX - 1);
		// strncpy(msgVal, msg, len + 1);

		const int LINE_HEIGHT = 16;
		XPLMDrawString(rgb, msgX, msgY, msgVal, NULL, xplmFont_Proportional);
		int y = msgY - LINE_HEIGHT;
		for (size_t i = 0; i < newLineCount; ++i)
		{
			XPLMDrawString(rgb, msgX, y, msgVal + newLines[i], NULL, xplmFont_Proportional);
			y -= LINE_HEIGHT;
		}

		XPLMDataRef throttle_dref = XPLMFindDataRef("sim/cockpit2/engine/actuators/throttle_ratio_all");
		float throttle = XPLMGetDataf(throttle_dref);
		XPLMDrawString(rgb, 100, 100, (char*)(std::to_string(throttle).c_str()), NULL, xplmFont_Proportional);
		

		if (numWaypoints == 0) {
			return 0;
		}
		
		float blue_x = XPLMGetDataf(ref_blueX);
		float blue_y = XPLMGetDataf(ref_blueY);
		float blue_z = XPLMGetDataf(ref_blueZ);

		float blue_heading_deg = XPLMGetDataf(ref_blueHeading_deg);
		float blue_pitch_deg = XPLMGetDataf(ref_bluePitch_deg);


		float red_x = XPLMGetDataf(ref_redX);
		float red_y = XPLMGetDataf(ref_redY);
		float red_z = XPLMGetDataf(ref_redZ);

		float red_heading_deg = XPLMGetDataf(ref_redHeading_deg);
		float red_pitch_deg = XPLMGetDataf(ref_redPitch_deg);


		// Xplane is not NEU
		float v_rel_x = -(red_x - blue_x);
		float v_rel_y = (red_z - blue_z);
		float v_rel_z = -(red_y - blue_y);

		float dist = sqrtf(v_rel_x*v_rel_x + v_rel_y*v_rel_y + v_rel_z*v_rel_z);
		Eigen::Vector3f v_rel = Eigen::Vector3f(v_rel_x/dist, v_rel_y/dist, v_rel_z/dist);
		Eigen::Vector3f v = Eigen::Vector3f(0,1,0);

		Eigen::Vector3f v_self = rot_mat_HP(blue_heading_deg, blue_pitch_deg*M_PI/180.0) * v;
		float trackAngle_deg = 180.0 - acosf(v_rel.transpose() * v_self) * 180.0/M_PI;


		gluProject(red_x, red_y, red_z, model_view, projection, viewport, &pos3D_x, &pos3D_y, &pos3D_z);


		double red_lat;
		double red_long;
		double red_altitude;
		XPLMLocalToWorld(blue_x, blue_y, blue_z, &red_lat, &red_long, &red_altitude); // g->latitude, g->longitude, g->altitude, &l->x, &l->y, &l->z);

		

		// If track angle is greater than 180 deg do not render
		if (trackAngle_deg > 90) {
			return 0;
		}


		float xoff = red_x - blue_x;
		float yoff = red_y - blue_y;
		float zoff = red_z - blue_z;


		float d = sqrtf(xoff*xoff + yoff * yoff + zoff * zoff);
		{
			static char v2[MSG_MAX] = { 0 };
			sprintf(v2, "%d", (int)(d*conversion_distance));
			XPLMDrawString(rgb_red, makerSize(d) + 5 + pos3D_x, pos3D_y, v2, NULL, xplmFont_Proportional);
		}

		uint64_t now_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		if(prevDistance_time == 0)
		{
			prevDistance_time = now_time;
			prevDistance_meter = d;
		}
		else if((now_time - prevDistance_time) >= 1000)
		{
			uint64_t delta_time = now_time - prevDistance_time;
			closureCalculation_m_sec = (prevDistance_meter - d)/((float)delta_time/1000.0);
			closureRateCalculated = true;
			prevDistance_time = now_time;
			prevDistance_meter = d;
		}

		if(closureRateCalculated == true)
		{
			static char v2[MSG_MAX] = { 0 };
			sprintf(v2, "%d", (int)(closureCalculation_m_sec*conversion_speed));
			XPLMDrawString(rgb_red, makerSize(d) + 5 + pos3D_x, pos3D_y-10, v2, NULL, xplmFont_Proportional);
		}


		{
			static char v2[MSG_MAX] = { 0 };
			sprintf(v2, "%d", (int)(trackAngle_deg));
			XPLMDrawString(rgb_red, makerSize(d) + 5 + pos3D_x, pos3D_y-20, v2, NULL, xplmFont_Proportional);
		}


		// print altitude
		{
			static char v2[MSG_MAX] = { 0 };
			sprintf(v2, "%d", (int)(red_altitude*conversion_distance));
			XPLMDrawString(rgb_red, -50 + pos3D_x, pos3D_y, v2, NULL, xplmFont_Proportional);
		}

			
		// Calculate target speed
		targetSpeedCalculated = true;
		bool SetTargetSpeedVariables = false;
		if(target_prev_time == 0) {
			SetTargetSpeedVariables = true;
		}
		else if((now_time - target_prev_time) >= 1000)
		{
			uint64_t delta_time = now_time - target_prev_time;
			float off_x = red_x - target_prev_pos_x;
			float off_y = red_y - target_prev_pos_y;
			float off_z = red_z - target_prev_pos_z;
			float dist = sqrtf(off_x*off_x + off_y*off_y + off_z*off_z);
			targetSpeed_m_sec = (dist)/((float)delta_time/1000.0);
			targetSpeedCalculated = true;
			SetTargetSpeedVariables = true;
		}

		if(SetTargetSpeedVariables == true)
		{
			target_prev_pos_x = red_x;
			target_prev_pos_y = red_y;
			target_prev_pos_z = red_z;
			target_prev_time = now_time;
		}

		if(targetSpeedCalculated == true)
		{
			static char v2[MSG_MAX] = { 0 };
			sprintf(v2, "%d", (int)(targetSpeed_m_sec*conversion_speed));
			XPLMDrawString(rgb_red, -50 + pos3D_x, pos3D_y-10, v2, NULL, xplmFont_Proportional);
		}

		return 0;
	}

	/// Draws waypoints.
	static int RouteDrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void * inRefcon)
	{
		if (numWaypoints == 0) {
			return 0;
		}

		
		glGetDoublev(GL_MODELVIEW_MATRIX, model_view);

		glGetDoublev(GL_PROJECTION_MATRIX, projection);

		glGetIntegerv(GL_VIEWPORT, viewport);


		float blue_x = XPLMGetDataf(ref_blueX);
		float blue_y = XPLMGetDataf(ref_blueY);
		float blue_z = XPLMGetDataf(ref_blueZ);

		float blue_heading_deg = XPLMGetDataf(ref_blueHeading_deg);
		float blue_pitch_deg = XPLMGetDataf(ref_bluePitch_deg);
		float blue_roll_deg = XPLMGetDataf(ref_blueRoll_deg);

		float red_x = XPLMGetDataf(ref_redX);
		float red_y = XPLMGetDataf(ref_redY);
		float red_z = XPLMGetDataf(ref_redZ);

		float red_heading_deg = XPLMGetDataf(ref_redHeading_deg);
		float red_pitch_deg = XPLMGetDataf(ref_redPitch_deg);
		float red_roll_deg = XPLMGetDataf(ref_redRoll_deg);

		// Xplane is not NEU
		float v_rel_x = -(red_x - blue_x);
		float v_rel_y = (red_z - blue_z);
		float v_rel_z = -(red_y - blue_y);
		float dist_m = sqrtf(v_rel_x*v_rel_x + v_rel_y * v_rel_y + v_rel_z * v_rel_z);

		Eigen::Matrix3f blue_rot;
		blue_rot = Eigen::AngleAxisf(-blue_heading_deg * M_PI / 180.0, Eigen::Vector3f::UnitY())
			* Eigen::AngleAxisf(blue_pitch_deg*M_PI / 180.0, Eigen::Vector3f::UnitX())
			* Eigen::AngleAxisf(-blue_roll_deg * M_PI / 180.0, Eigen::Vector3f::UnitZ());

		Eigen::Matrix3f red_rot;
		red_rot = Eigen::AngleAxisf(-red_heading_deg * M_PI / 180.0, Eigen::Vector3f::UnitY())
			* Eigen::AngleAxisf(red_pitch_deg*M_PI / 180.0, Eigen::Vector3f::UnitX())
			* Eigen::AngleAxisf(-red_roll_deg*M_PI / 180.0, Eigen::Vector3f::UnitZ());

		Eigen::Vector3f v_rel = Eigen::Vector3f(v_rel_x / dist_m, v_rel_y / dist_m, v_rel_z / dist_m);
		Eigen::Vector3f v = Eigen::Vector3f(0, 1, 0);

		Eigen::Vector3f v_self_track_angle = rot_mat_HP(blue_heading_deg, blue_pitch_deg*M_PI / 180.0) * v;
		float trackAngle_deg = 180.0 - acosf(v_rel.transpose() * v_self_track_angle) * 180.0 / M_PI;

		float score_dist = score_WEZ_max_ft * ft_to_meter;
		if (dist_m < score_WEZ_max_ft * ft_to_meter) {
			score_dist = dist_m;
		}


		Eigen::Vector3f blue_forward = blue_rot * Eigen::Vector3f(0, 0, -score_dist);
		Eigen::Vector3f red_forward = red_rot * Eigen::Vector3f(0, 0, -score_dist);

		glLineWidth(2.0);
		
		/*
		glColor3f(0.0F, 0.0F, 0.0F);
		glBegin(GL_LINES);
		glVertex3f((float)red_x, (float)red_y, (float)red_z);
		glVertex3f((float)red_x, -1000.0F, (float)red_z);
		glEnd();
		*/


		XPLMDataRef vr_dref = XPLMFindDataRef("sim/graphics/VR/enabled");
		bool vr_is_enabled = XPLMGetDatai(vr_dref);

		if (vr_is_enabled) {

			glColor3f(1.0F, 1.0F, 1.0F);
			glBegin(GL_LINES);
			glVertex3f((float)blue_x, (float)blue_y, (float)blue_z);
			glVertex3f((float)red_x, (float)red_y, (float)red_z);
			glEnd();


			

			// Draw line from self, to the score distance
			if(dist_m < 3000/meter_to_ft)
				glColor3f(0.0F, 1.0F, 0.0F);
			else
				glColor3f(0.0F, 0.0F, 0.0F);
			glBegin(GL_LINES);
			glVertex3f((float)blue_x, (float)blue_y, (float)blue_z);
			glVertex3f(blue_x + blue_forward[0], blue_y + blue_forward[1], blue_z + blue_forward[2]);
			glEnd();
		}


		

		Eigen::Vector3f red_back = red_rot * Eigen::Vector3f(0, 0, 10000);
		glColor3f(0.0F, 1.0F, 0.0F);
		glBegin(GL_LINES);
		glVertex3f((float)red_x, (float)red_y, (float)red_z);
		glVertex3f(red_x + red_back[0], red_y + red_back[1], red_z + red_back[2]);
		glEnd();

		Eigen::Vector3f red_down = red_rot * Eigen::Vector3f(0, 100, 0);
		glColor3f(1.0F, 0.0F, 0.0F);
		glBegin(GL_LINES);
		glVertex3f((float)red_x, (float)red_y, (float)red_z);
		glVertex3f(red_x + red_down[0], red_y + red_down[1], red_z + red_down[2]);
		glEnd();


		Eigen::Vector3f red_right = red_rot * Eigen::Vector3f(100, 0, 0);
		glColor3f(0.0F, 0.0F, 1.0F);
		glBegin(GL_LINES);
		glVertex3f((float)red_x, (float)red_y, (float)red_z);
		glVertex3f(red_x + red_right[0], red_y + red_right[1], red_z + red_right[2]);
		glEnd();

		if (trackAngle_deg > 90) {

			float p_x = blue_x + blue_forward[0] + (blue_x - (red_x + red_forward[0]));
			float p_y = blue_y + blue_forward[1] + (blue_y - (red_y + red_forward[1]));
			float p_z = blue_z + blue_forward[2];


			Eigen::Vector3f red_down = red_rot * Eigen::Vector3f(0, 5, 0);
			glColor3f(1.0F, 0.0F, 0.0F);
			glBegin(GL_LINES);
			glVertex3f(p_x, p_y, p_z);
			glVertex3f(p_x + red_down[0], p_y + red_down[1], p_z + red_down[2]);
			glEnd();


			Eigen::Vector3f red_right = red_rot * Eigen::Vector3f(5, 0, 0);
			glColor3f(0.0F, 0.0F, 1.0F);
			glBegin(GL_LINES);
			glVertex3f(p_x, p_y, p_z);
			glVertex3f(p_x + red_right[0], p_y + red_right[1], p_z + red_right[2]);
			glEnd();
		}

		// Draw WEZ Circle (really a sphere)
		float r = dist_m * (score_WEZ_angle_deg*M_PI / 180.0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if (dist_m < score_WEZ_max_ft * ft_to_meter)
			if(trackAngle_deg > 90)
				glColor4f(1.0F, 0.0F, 0.0F, 0.5);
			else
				glColor4f(0.0F, 1.0F, 0.0F, 0.5);
			
		else
			glColor4f(0.0F, 0.0F, 0.0F, 0.25);
		gl_drawSphere(blue_x + blue_forward[0], blue_y + blue_forward[1], blue_z + blue_forward[2], r);

		// If outside range, draw a sphere indicating the desired range
		if (dist_m >= score_WEZ_max_ft * ft_to_meter) {
			Eigen::Vector3f blue_forward = blue_rot * Eigen::Vector3f(0, 0, -score_WEZ_max_ft * ft_to_meter);
			glColor4f(0.0F, 0.0F, 1.0F, 0.25);
			gl_drawSphere(blue_x + blue_forward[0], blue_y + blue_forward[1], blue_z + blue_forward[2], score_WEZ_max_ft * ft_to_meter * (score_WEZ_angle_deg*M_PI / 180.0));
		}
		glDisable(GL_BLEND);


		///////////////////////////////////////////////////////////////////
		// Throttle bar
		///////////////////////////////////////////////////////////////////
		XPLMDataRef throttle_dref = XPLMFindDataRef("sim/cockpit2/engine/actuators/throttle_ratio_all");
		float throttle = XPLMGetDataf(throttle_dref);
		Eigen::Vector3f blue_throttleRender = blue_rot * Eigen::Vector3f(3.5, 0, -20);

		Eigen::Matrix3f roll_rot;
		roll_rot = Eigen::AngleAxisf(-0 * M_PI / 180.0, Eigen::Vector3f::UnitY())
			* Eigen::AngleAxisf(0 * M_PI / 180.0, Eigen::Vector3f::UnitX())
			* Eigen::AngleAxisf(-blue_roll_deg * M_PI / 180.0, Eigen::Vector3f::UnitZ());

		Eigen::Vector3f downThrottlePoint = blue_rot * Eigen::Vector3f(3.5, -1 * (1 - throttle), -20);
		Eigen::Vector3f downThrottleZero = blue_rot * Eigen::Vector3f(3.5, -1, -20);

		Eigen::Vector3f right(0, 0, 0);

		glColor4f(1.0F, 0.0F, 0.0F, 0.5);
		glLineWidth(10);
		glBegin(GL_LINES);
		glVertex3f(blue_x + blue_throttleRender[0], blue_y + blue_throttleRender[1], blue_z + blue_throttleRender[2]);
		glVertex3f(blue_x + downThrottlePoint[0], blue_y + downThrottlePoint[1], blue_z + downThrottlePoint[2]);
		glEnd();

		glColor4f(0.0F, 1.0F, 0.0F, 0.5);
		glLineWidth(10);
		glBegin(GL_LINES);
		glVertex3f(blue_x + downThrottlePoint[0], blue_y + downThrottlePoint[1], blue_z + downThrottlePoint[2]);
		glVertex3f(blue_x + downThrottleZero[0], blue_y + downThrottleZero[1], blue_z + downThrottleZero[2]);
		glEnd();

	

		return 0;
	}


	// Public Functions
	void Drawing::ClearMessage()
	{
		XPLMUnregisterDrawCallback(MessageDrawCallback, xplm_Phase_Window, 0, NULL);
		msgEnabled = false;
	}

	void Drawing::SetMessage(int x, int y, char* msg)
	{
		// Determine the size of the message and clear it if it is empty.
		size_t len = strnlen(msg, MSG_MAX - 1);
		if (len == 0)
		{
			ClearMessage();
			return;
		}

		// Set the message, location, and mark new lines.
		strncpy(msgVal, msg, len + 1);
		newLineCount = 0;
		for (size_t i = 0; i < len && newLineCount < MSG_LINE_MAX; ++i)
		{
			if (msgVal[i] == '\n' || msgVal[i] == '\r')
			{
				msgVal[i] = 0;
				newLines[newLineCount++] = i + 1;
			}
		}
		msgX = x < 0 ? 10 : x;
		msgY = y < 0 ? 600 : y;

		// Enable drawing if necessary
		if (!msgEnabled)
		{
			XPLMRegisterDrawCallback(MessageDrawCallback, xplm_Phase_Window, 0, NULL);
			msgEnabled = true;
		}
	}

	void Drawing::ClearWaypoints()
	{
		numWaypoints = 0;
		if (routeEnabled)
		{
			XPLMUnregisterDrawCallback(RouteDrawCallback, xplm_Phase_Modern3D, 0, NULL);
			ClearMessage();
		}
		return;
	}

	void Drawing::AddWaypoints(Waypoint points[], size_t numPoints)
	{
		if (numWaypoints + numPoints > WAYPOINT_MAX)
		{
			numPoints = WAYPOINT_MAX - numWaypoints;
		}
		size_t finalNumWaypoints = numPoints + numWaypoints;
		for (size_t i = 0; i < numPoints; ++i)
		{
			waypoints[numWaypoints + i] = points[i];
		}
		numWaypoints = finalNumWaypoints;

		if (!routeEnabled)
		{
			XPLMRegisterDrawCallback(RouteDrawCallback, xplm_Phase_Modern3D, 0, NULL);
		}
		if (!ref_blueX)
		{
			ref_blueX = XPLMFindDataRef("sim/flightmodel/position/local_x");
			ref_blueY = XPLMFindDataRef("sim/flightmodel/position/local_y");
			ref_blueZ = XPLMFindDataRef("sim/flightmodel/position/local_z");

			ref_bluePitch_deg = XPLMFindDataRef("sim/flightmodel/position/theta");
			ref_blueHeading_deg = XPLMFindDataRef("sim/flightmodel/position/psi");
			ref_blueRoll_deg = XPLMFindDataRef("sim/flightmodel/position/phi");

			ref_redX = XPLMFindDataRef("sim/multiplayer/position/plane1_x");
			ref_redY = XPLMFindDataRef("sim/multiplayer/position/plane1_y");
			ref_redZ = XPLMFindDataRef("sim/multiplayer/position/plane1_z");

			ref_redPitch_deg = XPLMFindDataRef("sim/multiplayer/position/plane1_the");
			ref_redHeading_deg = XPLMFindDataRef("sim/multiplayer/position/plane1_psi");
			ref_redRoll_deg = XPLMFindDataRef("sim/multiplayer/position/plane1_phi");

			ref_cam_pitch = XPLMFindDataRef("sim/graphics/view/view_pitch");
			ref_cam_vert_FOV_deg = XPLMFindDataRef("sim/graphics/view/field_of_view_deg");
		}
	}

	void Drawing::RemoveWaypoints(Waypoint points[], size_t numPoints)
	{
		// Build a list of indices of waypoints we should delete.
		size_t delPoints[WAYPOINT_MAX];
		size_t delPointsCur = 0;
		for (size_t i = 0; i < numPoints; ++i)
		{
			Waypoint p = points[i];
			for (size_t j = 0; j < numWaypoints; ++j)
			{
				Waypoint q = waypoints[j];
				if (p.latitude == q.latitude &&
					p.longitude == q.longitude &&
					p.altitude == q.altitude)
				{
					delPoints[delPointsCur++] = j;
					break;
				}
			}
		}
		// Sort the indices so that we only have to iterate them once
		qsort(delPoints, delPointsCur, sizeof(size_t), cmp);

		// Copy the new array on top of the old array
		size_t copyCur = 0;
		size_t count = delPointsCur;
		delPointsCur = 0;
		for (size_t i = 0; i < numWaypoints; ++i)
		{
			if (i == delPoints[delPointsCur])
			{
				++delPointsCur;
				continue;
			}
			waypoints[copyCur++] = waypoints[i];
		}
		numWaypoints -= count;
		if (numWaypoints == 0)
		{
			ClearWaypoints();
		}
	}
}
