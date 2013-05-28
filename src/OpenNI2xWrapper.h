/*
	CADET - Center for Advances in Digital Entertainment Technologies
	Copyright 2013 University of Applied Science Salzburg / MultiMediaTechnology

	http://www.cadet.at
	http://multimediatechnology.at/

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

	CADET - Center for Advances in Digital Entertainment Technologies

	Authors: Robert Praxmarer
	Email: support@cadet.at
	Created: 17-05-2013
*/

#pragma once

#include "cinder/app/AppBasic.h"
#include "cinder/Surface.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/gl.h"
#include <OpenNI.h>
#include <Nite.h>


#define MAX_USERS 10

class OpenNIDevice
{
	public:	
		openni::Device								m_Device;
		openni::VideoStream**						m_pStreams;
		openni::VideoStream							m_RGBStream;
		openni::VideoStream							m_IRStream;
		openni::VideoStream							m_DepthStream;
		openni::VideoFrameRef						m_DepthFrame;
		openni::VideoFrameRef						m_RGBFrame;
		openni::VideoFrameRef						m_IRFrame;
		nite::UserTrackerFrameRef					m_UserTrackerFrame;
		nite::UserTracker*							m_pUserTracker;				// this has to be a pointer otherwise the shutdown won't work as you would expect
		nite::UserId								m_poseUser;
		uint64_t									m_poseTime;
		uint16_t									m_UserCount;
		openni::PlaybackControl*					m_Player;
		
		std::mutex									m_MutexDevice;	

		bool					m_bVisibleUsers[MAX_USERS];
		nite::SkeletonState		m_SkeletonStates[MAX_USERS];
		char					m_cUserStatusLabels[MAX_USERS][100];

		uint16_t		m_iRgbImgWidth;
		uint16_t		m_iRgbImgHeight;
		uint16_t		m_iIRImgWidth;
		uint16_t		m_iIRImgHeight;
		uint16_t		m_iDepthImgWidth;
		uint16_t		m_iDepthImgHeight;
		uint16_t		m_iUserImgWidth;
		uint16_t		m_iUserImgHeight;

		bool	m_bRGBStreamActive;
		bool	m_bIRStreamActive;
		bool	m_bDepthStreamActive;
		bool	m_bUserStreamActive;
		bool	m_bSubtractBackground;

		ci::Surface								m_RGBSurface;
		ci::Surface								m_IRSurface;
		ci::Surface16u							m_Depth16BitSurface;
		ci::Surface								m_Depth8BitSurface;
		ci::Surface								m_UserSurface;
		ci::Surface16u							m_BackgroundSurface;
		ci::Surface16u							m_Depth16BitUserMask;
		ci::Surface								m_Depth8BitUserMaskSurface;

		ci::gl::Texture							m_IRTexture;
		ci::gl::Texture							m_RGBTexture;
		ci::gl::Texture							m_Depth16BitTexture;
		ci::gl::Texture							m_Depth8BitTexture;
		ci::gl::Texture							m_DepthDiffHistogramTexture;
		ci::gl::Texture							m_UserTexture;
		ci::gl::Texture							m_DepthDiffTexture;
};


class OpenNI2xWrapper : public openni::OpenNI::DeviceConnectedListener,
									public openni::OpenNI::DeviceDisconnectedListener,
									public openni::OpenNI::DeviceStateChangedListener
{
public:
	OpenNI2xWrapper(void);
	~OpenNI2xWrapper(void);

	bool			init(bool bUseUserTracking=true);
	bool			shutdown();
	bool			startDevice(uint16_t iDeviceNumber, bool bHasRGBStream=true, bool bHasDepthStream=true, bool bHasUserTracker=true, bool bHasIRStream=false);
	void			stopDevice(uint16_t iDeviceNumber);
	void			updateDevice(uint16_t iDeviceNumber);
	bool			resetDevice(uint16_t iDeviceNumber);
	uint16_t		getNumberOfConnectedDevices();
	uint16_t		getDeviceNumberForURI(std::string uri);
	uint16_t		getNumberOfRunningDevices();
	uint16_t		getNumberOfUsers(uint16_t iDeviceNumber);
	
	bool			startRecording(uint16_t iDeviceNumber, std::string fileName, bool isLossyCompressed=false);
	void			stopRecording();
	uint16_t		startPlayback(std::string fileName);
	void			stopPlayback(uint16_t iRecordId);
	
	void			setDepthColorImageAlignment(uint16_t iDeviceNumber, bool bEnabled);
	void			setAllStreamsMirrored(uint16_t iDeviceNumber, bool bEnabled);
	void			setBackgroundSubtraction(uint16_t iDeviceNumber, bool bEnabled);
	void			setDepthColorSync(uint16_t iDeviceNumber, bool bEnabled);
	
	ci::Surface		getDepth8BitSurface(uint16_t iDeviceNumber);
	ci::gl::Texture getDepth8BitTexture(uint16_t iDeviceNumber);
	ci::Surface16u	getDepth16BitSurface(uint16_t iDeviceNumber);
	ci::gl::Texture getDepth16BitTexture(uint16_t iDeviceNumber);
	ci::Surface		getRGBSurface(uint16_t iDeviceNumber);
	ci::gl::Texture getRGBTexture(uint16_t iDeviceNumber);
	ci::Surface		getIRSurface(uint16_t iDeviceNumber);
	ci::gl::Texture getIRTexture(uint16_t iDeviceNumber);
	ci::Surface		getUserSurface(uint16_t iDeviceNumber);
	ci::gl::Texture getUserTexture(uint16_t iDeviceNumber);
	
	ci::Vec3f					getCenterOfMassOfUser(uint16_t iDeviceNumber, uint16_t iUserID);
	void						drawSkeletons(uint16_t iDeviceNumber, ci::Rectf rect);
	std::vector<cinder::Vec3f>	getSkeletonJointPositions(uint16_t iDeviceNumber, uint16_t iUser);				//in normalized screen coords 0..1
	void						debugDraw(uint16_t iDeviceNumber);
	
private:
	bool startStreams(uint16_t iDeviceNumber, bool bHasRGBStream, bool bHasDepthStream, bool bHasUserTracker, bool hasIRStream);
	void printUserState(uint16_t iDeviceNumber, const nite::UserData& user, uint64_t ts);

	// overwrite for baseclass of openni for device connection callbacks
	void onDeviceStateChanged(const openni::DeviceInfo* pInfo, openni::DeviceState state);
	void onDeviceConnected(const openni::DeviceInfo* pInfo);
	void onDeviceDisconnected(const openni::DeviceInfo* pInfo);

	std::vector<std::shared_ptr<OpenNIDevice>>			m_Devices;
	openni::Array<openni::DeviceInfo>					m_DeviceInfoList;	
	openni::Recorder									m_Recorder;
	std::mutex											m_Mutex;	
	
	bool												m_bUserTrackingInitizialized;
};

