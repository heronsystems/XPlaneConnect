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
#include "Drawing.h"

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"

#include <cmath>
#include <string>
#include <cstring>
#include <array>
// OpenGL includes

#define GL_GLEXT_PROTOTYPES
#if IBM
#include <windows.h>
#endif
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include <chrono>
#include <cstdint>

#include "./common/shader_utils.h"

#include <Eigen/Dense>
#include <Eigen/Geometry> 

#include <ft2build.h>
#include FT_FREETYPE_H

FT_Library ft;
FT_Face face;

GLuint vbo;

GLuint program;
GLint attribute_coord;
GLint uniform_tex;
GLint uniform_color;

struct point {
	GLfloat x;
	GLfloat y;
	GLfloat s;
	GLfloat t;
};

double TEST = 5;


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
	static float rgb2[3] = { 1.0F, 0.0F, 0.0F };

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

	XPLMDataRef ref_cam_x;
	XPLMDataRef	ref_cam_y;
	XPLMDataRef	ref_cam_z;

	XPLMDataRef ref_cam_pitch;
	XPLMDataRef ref_cam_vert_FOV_deg;

	XPLMDataRef ref_window_height;


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


	static void render_text(const char *text, float x, float y, float sx, float sy) {
		const char *p;
		FT_GlyphSlot g = face->glyph;

		/* Create a texture that will be used to hold one "glyph" */
		GLuint tex;

		glActiveTexture(GL_TEXTURE0);
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(uniform_tex, 0);

		/* We require 1 byte alignment when uploading texture data */
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		/* Clamping to edges is important to prevent artifacts when scaling */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		/* Linear filtering usually looks best for text */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		/* Set up the VBO for our vertex data */
		glEnableVertexAttribArray(attribute_coord);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
		/* Loop through all characters */
		for (p = text; *p; p++) {
			/* Try to load and render the character */
			if (FT_Load_Char(face, *p, FT_LOAD_RENDER))
				continue;

			/* Upload the "bitmap", which contains an 8-bit grayscale image, as an alpha texture */
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, g->bitmap.width, g->bitmap.rows, 0, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);

			/* Calculate the vertex and texture coordinates */
			float x2 = x + g->bitmap_left * sx;
			float y2 = -y - g->bitmap_top * sy;
			float w = g->bitmap.width * sx;
			float h = g->bitmap.rows * sy;

			point box[4] = {
				{x2, -y2, 0, 0},
				{x2 + w, -y2, 1, 0},
				{x2, -y2 - h, 0, 1},
				{x2 + w, -y2 - h, 1, 1},
			};

			/* Draw the character on the screen */
			glBufferData(GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			/* Advance the cursor to the start of the next character */
			x += (g->advance.x >> 6) * sx;
			y += (g->advance.y >> 6) * sy;
		}

		glDisableVertexAttribArray(attribute_coord);
		glDeleteTextures(1, &tex);

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

		float cx = XPLMGetDataf(ref_cam_x);
		float cy = XPLMGetDataf(ref_cam_y);
		float cz = XPLMGetDataf(ref_cam_z);

		Eigen::Matrix3f blue_rot;
		blue_rot = Eigen::AngleAxisf(blue_heading_deg*M_PI/180.0,  Eigen::Vector3f::UnitZ())
  				   * Eigen::AngleAxisf(blue_pitch_deg*M_PI/180.0,   Eigen::Vector3f::UnitY());

		Eigen::Matrix3f red_rot;
		red_rot = Eigen::AngleAxisf(red_heading_deg*M_PI/180.0,  Eigen::Vector3f::UnitZ())
  				   * Eigen::AngleAxisf(red_pitch_deg*M_PI/180.0,   Eigen::Vector3f::UnitY());

		Eigen::Matrix3f T = blue_rot * red_rot.transpose();


		// Xplane is not NEU
		float v_rel_x = -(red_x - blue_x);
		float v_rel_y = (red_z - blue_z);
		float v_rel_z = -(red_y - blue_y);

		float dist = sqrtf(v_rel_x*v_rel_x + v_rel_y*v_rel_y + v_rel_z*v_rel_z);
		Eigen::Vector3f v_rel = Eigen::Vector3f(v_rel_x/dist, v_rel_y/dist, v_rel_z/dist);
		Eigen::Vector3f v = Eigen::Vector3f(0,1,0);

		//a : vector of heading and pitch angles in order.  Heading taken in degrees, pitch in rad
    	//    returns a 3x3 rotation matrix for Heading, Pitch.  Assume vector aligned with roll axis
		auto rot_mat_HP = [](float a0, float a1) {
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
		Eigen::Vector3f v_self = rot_mat_HP(blue_heading_deg, blue_pitch_deg*M_PI/180.0) * v;
		float trackAngle_deg = 180.0 - acosf(v_rel.transpose() * v_self) * 180.0/M_PI;


		for(size_t i = 0 ; i < numWaypoints; i++) {

			Waypoint* g = &waypoints[i];
			LocalPoint* l = &localPoints[i];
			XPLMWorldToLocal(g->latitude, g->longitude, g->altitude,
				&l->x, &l->y, &l->z);


			gluProject(l->x, l->y, l->z,	model_view, projection, viewport,	&pos3D_x, &pos3D_y, &pos3D_z);


			// If track angle is greater than 180 deg do not render
			if(trackAngle_deg > 90) {
				continue;
			}

			float xoff = (float)l->x - blue_x;
			float yoff = (float)l->y - blue_y;
			float zoff = (float)l->z - blue_z;
			float d = sqrtf(xoff*xoff + yoff*yoff + zoff*zoff);
			{
				const char* msg2 = std::to_string((int)(d*3.28084)).c_str();
				static char v2[MSG_MAX] = { 0 };
				size_t len2 = strnlen(msg2, MSG_MAX - 1);
				strncpy(v2, msg2, len2 + 1);
				XPLMDrawString(rgb2, makerSize(d) + 5 + pos3D_x, pos3D_y, v2, NULL, xplmFont_Proportional);
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
				const char* msg2 = std::to_string((int)(closureCalculation_m_sec*3.28084)).c_str();
				static char v2[MSG_MAX] = { 0 };
				size_t len2 = strnlen(msg2, MSG_MAX - 1);
				strncpy(v2, msg2, len2 + 1);
				XPLMDrawString(rgb2, makerSize(d) + 5 + pos3D_x, pos3D_y-10, v2, NULL, xplmFont_Proportional);
			}


			{
				const char* msg2 = std::to_string((int)(trackAngle_deg)).c_str();
				static char v2[MSG_MAX] = { 0 };
				size_t len2 = strnlen(msg2, MSG_MAX - 1);
				strncpy(v2, msg2, len2 + 1);
				XPLMDrawString(rgb2, makerSize(d) + 5 + pos3D_x, pos3D_y-20, v2, NULL, xplmFont_Proportional);
			}


			// print altitude
			{
				const char* msg2 = std::to_string((int)(g->altitude*3.28084)).c_str();
				static char v2[MSG_MAX] = { 0 };
				size_t len2 = strnlen(msg2, MSG_MAX - 1);
				strncpy(v2, msg2, len2 + 1);
				XPLMDrawString(rgb2, -50 + pos3D_x, pos3D_y, v2, NULL, xplmFont_Proportional);
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
				float off_x = (float)l->x - target_prev_pos_x;
				float off_y = (float)l->y - target_prev_pos_y;
				float off_z = (float)l->z - target_prev_pos_z;
				float dist = sqrtf(off_x*off_x + off_y*off_y + off_z*off_z);
				targetSpeed_m_sec = (dist)/((float)delta_time/1000.0);
				targetSpeedCalculated = true;
				SetTargetSpeedVariables = true;
			}

			if(SetTargetSpeedVariables == true)
			{
				target_prev_pos_x = l->x;
				target_prev_pos_y = l->y;
				target_prev_pos_z = l->z;
				target_prev_time = now_time;
			}

			if(targetSpeedCalculated == true)
			{
				const char* msg2 = std::to_string((int)(targetSpeed_m_sec*3.28084)).c_str();
				static char v2[MSG_MAX] = { 0 };
				size_t len2 = strnlen(msg2, MSG_MAX - 1);
				strncpy(v2, msg2, len2 + 1);
				XPLMDrawString(rgb2, -50 + pos3D_x, pos3D_y-10, v2, NULL, xplmFont_Proportional);
			}
			
			return 1;
		}
	}

	/// Draws waypoints.
	static int RouteDrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void * inRefcon)
	{
		


		float blue_x = XPLMGetDataf(ref_blueX);
		float blue_y = XPLMGetDataf(ref_blueY);
		float blue_z = XPLMGetDataf(ref_blueZ);

		for(size_t i = 0 ; i < numWaypoints; i++) {
			Waypoint* g = &waypoints[i];
			LocalPoint* l = &localPoints[i];
			XPLMWorldToLocal(g->latitude, g->longitude, g->altitude,
				&l->x, &l->y, &l->z);

			
			glGetDoublev(GL_MODELVIEW_MATRIX, model_view);
			
			glGetDoublev(GL_PROJECTION_MATRIX, projection);
			
			glGetIntegerv(GL_VIEWPORT, viewport);	

			gluProject(blue_x, blue_y, blue_z,	model_view, projection, viewport,	&pos3D_x, &pos3D_y, &pos3D_z);


			// Draw posts
			glColor3f(1.0F, 1.0F, 1.0F);
			glBegin(GL_LINES);
			glVertex3f((float)l->x, (float)l->y, (float)l->z);
			glVertex3f((float)l->x, -1000.0F, (float)l->z);
			glEnd();

			// Draw route
			// glColor3f(1.0F, 1.0F, 0.0F);
			// glBegin(GL_LINE_STRIP);
			// glVertex3f((float)l->x, (float)l->y, (float)l->z);
			// glVertex3f(blue_x, blue_y, blue_z);
			// glEnd();

			
			// Draw markers
			float xoff = (float)l->x - blue_x;
			float yoff = (float)l->y - blue_y;
			float zoff = (float)l->z - blue_z;
			float d = sqrtf(xoff*xoff + yoff*yoff + zoff*zoff);

			float f = 1.0F;
			if(d < 8000) {
				f = (d/8000);
			}

			glColor3f(1.0F, f, f);
			gl_drawCube((float)l->x, (float)l->y, (float)l->z, d);
		}

		// int screen_width=800, screen_height=600;
		// float sx = 2.0 / screen_width;
		// float sy = 2.0 / screen_height;


		// if(FT_Init_FreeType(&ft)) {
		// 	fprintf(stderr, "Could not init freetype library\n");
		// 	return 1;
		// }

		
		// if(FT_New_Face(ft, "Resources/plugins/XPlaneConnect/FreeSans.ttf", 0, &face)) {
		// 	fprintf(stderr, "Could not open font\n");
		// 	return 1;
		// }
		// program = create_program("Resources/plugins/XPlaneConnect/text.v.glsl", "Resources/plugins/XPlaneConnect/text.f.glsl");
		// if(program == 0)
		// 	return 1;

		// attribute_coord = get_attrib(program, "coord");
		// uniform_tex = get_uniform(program, "tex");
		// uniform_color = get_uniform(program, "color");

		// if(attribute_coord == -1 || uniform_tex == -1 || uniform_color == -1)
		// 	return false;

		// // Create the vertex buffer object
		// glGenBuffers(1, &vbo);

		// GLfloat black[4] = { 0, 0, 0, 1 };
		// GLfloat red[4] = { 1, 0, 0, 1 };
		// GLfloat transparent_green[4] = { 0, 1, 0, 0.5 };


		// FT_Set_Pixel_Sizes(face, 0, 48);
		// glUniform4fv(uniform_color, 1, black);
		// render_text("AAAAAAAAAAAAAAAA", -1 + 8 * sx, 1 - 50 * sy, sx, sy);
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
			XPLMUnregisterDrawCallback(RouteDrawCallback, xplm_Phase_Objects, 0, NULL);
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
			XPLMRegisterDrawCallback(RouteDrawCallback, xplm_Phase_Objects, 0, NULL);
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

			ref_cam_x = XPLMFindDataRef("sim/graphics/view/view_x");
			ref_cam_y = XPLMFindDataRef("sim/graphics/view/view_y");
			ref_cam_z = XPLMFindDataRef("sim/graphics/view/view_z");

			ref_cam_pitch = XPLMFindDataRef("sim/graphics/view/view_pitch");
			ref_cam_vert_FOV_deg = XPLMFindDataRef("sim/graphics/view/field_of_view_deg");

			ref_window_height = XPLMFindDataRef("sim/graphics/view/window_height");
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
