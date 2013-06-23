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
#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

class OpenNIDevice
{
	public:	
		OpenNIDevice()
		{
			// default initialization
			m_pDepth16BitRawPtr = nullptr;
			m_pDepth8BitRawPtr = nullptr;
			m_pRgbRawPtr = nullptr;
			m_pIrRawPtr = nullptr;
			m_pUserRawPtr = nullptr;
			m_pUserTracker = nullptr;
			m_bRGBStreamActive = false;
			m_bIRStreamActive = false;
			m_bDepthStreamActive = false;
			m_bUserStreamActive = false;
			m_bSubtractBackground = false;
			m_IRTexture = ci::gl::Texture();
			m_RGBTexture = ci::gl::Texture();
			m_Depth16BitTexture = ci::gl::Texture();
			m_Depth8BitTexture = ci::gl::Texture();
			m_DepthDiffHistogramTexture = ci::gl::Texture();
			m_UserTexture = ci::gl::Texture();
			m_DepthDiffTexture = ci::gl::Texture();
			m_iDeviceState = openni::DeviceState::DEVICE_STATE_OK;
		};

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
		openni::DeviceState							m_iDeviceState;
		bool										m_bIsDeviceActive;
		std::string									m_Uri;

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

		uint16_t*		m_pDepth16BitRawPtr;
		uint8_t*		m_pDepth8BitRawPtr;
		uint8_t*		m_pRgbRawPtr;
		uint8_t*		m_pIrRawPtr;
		uint16_t*		m_pUserRawPtr;

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

class OpenNIJoint
{
public:
	cinder::Vec3f m_Position;
	cinder::Quatf m_Orientation;
	float m_PositionConfidence;
	float m_OrientationConfidence;
};

class OpenNI2xWrapper : public openni::OpenNI::DeviceConnectedListener,
									public openni::OpenNI::DeviceDisconnectedListener,
									public openni::OpenNI::DeviceStateChangedListener
{
public:
	static OpenNI2xWrapper& getInstance();
	~OpenNI2xWrapper(void);

	bool			init(bool bUseUserTracking=true);
	bool			shutdown();
	int16_t			startDevice(std::string uri, bool bHasRGBStream=true, bool bHasDepthStream=true, bool bHasUserTracker=true, bool bHasIRStream=false);
	int16_t			startDevice(uint16_t iDeviceNumber, bool bHasRGBStream=true, bool bHasDepthStream=true, bool bHasUserTracker=true, bool bHasIRStream=false);
	void			stopDevice(std::string uri);
	void			stopDevice(uint16_t iDeviceNumber);
	void			pauseDevice(uint16_t iDeviceNumber);
	void			resumeDevice(uint16_t iDeviceNumber);
	void			updateDevice(uint16_t iDeviceNumber);
	bool			resetDevice(uint16_t iDeviceNumber);
	uint16_t		getDevicesConnected();
	int16_t			getRegisteredDeviceNumberForURI(std::string uri);
	uint16_t		getDevicesInitialized();
	bool			isDeviceActive(uint16_t iDeviceNumber);
	bool			isDeviceRunning(uint16_t iDeviceNumber);
	
	bool			startRecording(uint16_t iDeviceNumber, std::string fileName, bool isLossyCompressed=false);
	void			stopRecording();
	int16_t			startPlayback(std::string fileName, bool bLoop);
	void			stopPlayback(uint16_t iRecordId);
	void			pausePlayback(uint16_t iRecordId);
	void			resumePlayback(uint16_t iRecordId);
	bool			setPlaybackSpeed(uint16_t iRecordId, float speed);
	bool			setPlaybackRgbFrameNumber(uint16_t iRecordId, unsigned long frame);
	bool			setPlaybackIrFrameNumber(uint16_t iRecordId, unsigned long frame);
	bool			setPlaybackDepthFrameNumber(uint16_t iRecordId, unsigned long frame);
	unsigned long	getPlaybackNumberOfRgbFrames(uint16_t iRecordId);
	unsigned long	getPlaybackNumberOfIrFrames(uint16_t iRecordId);
	unsigned long	getPlaybackNumberOfDepthFrames(uint16_t iRecordId);	

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
	
	uint16_t		getRgbWidth(uint16_t iDeviceNumber);
	uint16_t		getRgbHeight(uint16_t iDeviceNumber);
	uint16_t		getIrWidth(uint16_t iDeviceNumber);
	uint16_t		getIrHeight(uint16_t iDeviceNumber);
	uint16_t		getDepthWidth(uint16_t iDeviceNumber);
	uint16_t		getDepthHeight(uint16_t iDeviceNumber);
	uint16_t		getUserWidth(uint16_t iDeviceNumber);
	uint16_t		getUserHeight(uint16_t iDeviceNumber);
	bool			setRgbResolution(uint16_t iDeviceNumber, uint16_t w, uint16_t h);
	bool			setDepthResolution(uint16_t iDeviceNumber, uint16_t w, uint16_t h);
	bool			setIrResolution(uint16_t iDeviceNumber, uint16_t w, uint16_t h);

	uint16_t*		getDepth16BitImgPtr(uint16_t iDeviceNumber);
	uint8_t*		getDepth8BitImgPtr(uint16_t iDeviceNumber);
	uint8_t*		getRgbImgPtr(uint16_t iDeviceNumber);
	uint8_t*		getIrImgPtr(uint16_t iDeviceNumber);
	uint16_t*		getUserImgPtr(uint16_t iDeviceNumber);

	uint16_t							getUserCount(uint16_t iDeviceNumber);
	bool								isOneUserVisible(uint16_t iDeviceNumber);
	bool								isUserVisible(uint16_t iDeviceNumber, uint16_t iUserID);
	ci::Vec3f							getUserCenterOfMass(uint16_t iDeviceNumber, uint16_t iUserID);						//in normalized screen coords 0..1
	ci::Rectf							getUserBoundingBox(uint16_t iDeviceNumber, uint16_t iUserID);						//in normalized screen coords 0..1
	std::vector<OpenNIJoint>			getUserSkeletonJoints(uint16_t iDeviceNumber, uint16_t iUser);						//in normalized screen coords 0..1

	void						drawSkeletons(uint16_t iDeviceNumber, ci::Rectf rect);
	void						debugDraw(uint16_t iDeviceNumber);
	
private:
	OpenNI2xWrapper(void);
	OpenNI2xWrapper(OpenNI2xWrapper& other){};

	bool startStreams(uint16_t iDeviceNumber, bool bHasRGBStream, bool bHasDepthStream, bool bHasUserTracker, bool hasIRStream);
	void printUserState(uint16_t iDeviceNumber, const nite::UserData& user, uint64_t ts);
	void convertToCinder(std::shared_ptr<OpenNIDevice> device);

	// overwrite for baseclass of openni for device connection callbacks
	void onDeviceStateChanged(const openni::DeviceInfo* pInfo, openni::DeviceState state);
	void onDeviceConnected(const openni::DeviceInfo* pInfo);
	void onDeviceDisconnected(const openni::DeviceInfo* pInfo);

	std::vector<std::shared_ptr<OpenNIDevice>>			m_Devices;
	openni::Array<openni::DeviceInfo>					m_DeviceInfoList;	
	openni::Recorder									m_Recorder;
	std::recursive_mutex								m_Mutex;	
	bool												m_bUserTrackingInitizialized;
};

