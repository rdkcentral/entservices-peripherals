/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "VoiceControl.h"
#include "libIBusDaemon.h"
#include <stdlib.h>
#include <memory>
#include "UtilsJsonRpc.h"
#include "UtilsIarm.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 4
#define API_VERSION_NUMBER_PATCH 0

using namespace std;

namespace WPEFramework {

    namespace {

        static Plugin::Metadata<Plugin::VoiceControl> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
        
        // RAII wrapper for ctrlm_voice_iarm_call_json_t memory management
        struct VoiceIARMCallGuard {
            ctrlm_voice_iarm_call_json_t* call;
            
            explicit VoiceIARMCallGuard(size_t size) : call(nullptr) {
                call = static_cast<ctrlm_voice_iarm_call_json_t*>(calloc(1, size));
            }
            
            ~VoiceIARMCallGuard() {
                if (call) {
                    free(call);
                    call = nullptr;
                }
            }
            
            ctrlm_voice_iarm_call_json_t* get() { return call; }
            bool isValid() const { return call != nullptr; }
            
            // Prevent copying
            VoiceIARMCallGuard(const VoiceIARMCallGuard&) = delete;
            VoiceIARMCallGuard& operator=(const VoiceIARMCallGuard&) = delete;
        };
    }

    namespace Plugin {

        SERVICE_REGISTRATION(VoiceControl, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        VoiceControl* VoiceControl::_instance = nullptr;

        VoiceControl::VoiceControl()
            : PluginHost::JSONRPC()
            , m_apiVersionNumber(0)
            , m_hasOwnProcess(false)
            , m_maskPii(false)
        {
            LOGINFO("ctor");
            VoiceControl::_instance = this;

            Register("getApiVersionNumber",          &VoiceControl::getApiVersionNumber,          this);

            Register("voiceStatus",                  &VoiceControl::voiceStatus,                  this);
            Register("configureVoice",               &VoiceControl::configureVoice,               this);
            Register("setVoiceInit",                 &VoiceControl::setVoiceInit,                 this);
            Register("sendVoiceMessage",             &VoiceControl::sendVoiceMessage,             this);
            Register("voiceSessionByText",           &VoiceControl::voiceSessionByText,           this);
            Register("voiceSessionTypes",            &VoiceControl::voiceSessionTypes,            this);
            Register("voiceSessionRequest",          &VoiceControl::voiceSessionRequest,          this);
            Register("voiceSessionTerminate",        &VoiceControl::voiceSessionTerminate,        this);
            Register("voiceSessionAudioStreamStart", &VoiceControl::voiceSessionAudioStreamStart, this);

            setApiVersionNumber(1);
        }

        VoiceControl::~VoiceControl()
        {
            //LOGINFO("dtor");
        }

        const string VoiceControl::Initialize(PluginHost::IShell*  /* service */)
        {
            InitializeIARM();
            
            // Initialize m_maskPii - handle failure gracefully
            try {
                getMaskPii_();
            } catch (const std::exception& e) {
                LOGERR("Exception during getMaskPii_: %s. Defaulting m_maskPii to false.", e.what());
                m_maskPii = false;
            } catch (...) {
                LOGERR("Unknown exception during getMaskPii_. Defaulting m_maskPii to false.");
                m_maskPii = false;
            }
            
            // On success return empty, to indicate there is no error text.
            return (string());
        }

        void VoiceControl::Deinitialize(PluginHost::IShell* /* service */)
        {
            LOGINFO("Deinitialize");
            DeinitializeIARM();
            VoiceControl::_instance = nullptr;
        }

        void VoiceControl::InitializeIARM()
        {
            if (Utils::IARM::init())
            {
                // We have our own Linux process, so we need to connect and disconnect from the IARM Bus
                m_hasOwnProcess = true;

                IARM_Result_t res;
                res = IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SESSION_BEGIN, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to register SESSION_BEGIN event handler: %d", res);
                }
                res = IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_STREAM_BEGIN, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to register STREAM_BEGIN event handler: %d", res);
                }
                res = IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_KEYWORD_VERIFICATION, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to register KEYWORD_VERIFICATION event handler: %d", res);
                }
                res = IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SERVER_MESSAGE, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to register SERVER_MESSAGE event handler: %d", res);
                }
                res = IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_STREAM_END, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to register STREAM_END event handler: %d", res);
                }
                res = IARM_Bus_RegisterEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SESSION_END, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to register SESSION_END event handler: %d", res);
                }
            }
            else
                m_hasOwnProcess = false;
        }

        //TODO(MROLLINS) - we need to install crash handler to ensure DeinitializeIARM gets called
        void VoiceControl::DeinitializeIARM()
        {
            if (m_hasOwnProcess)
            {
                IARM_Result_t res;
                res = IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SESSION_END, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to remove SESSION_END event handler: %d", res);
                }
                res = IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_STREAM_END, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to remove STREAM_END event handler: %d", res);
                }
                res = IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_KEYWORD_VERIFICATION, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to remove KEYWORD_VERIFICATION event handler: %d", res);
                }
                res = IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SERVER_MESSAGE, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to remove SERVER_MESSAGE event handler: %d", res);
                }
                res = IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_STREAM_BEGIN, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to remove STREAM_BEGIN event handler: %d", res);
                }
                res = IARM_Bus_RemoveEventHandler(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_EVENT_JSON_SESSION_BEGIN, voiceEventHandler);
                if (res != IARM_RESULT_SUCCESS) {
                    LOGERR("Failed to remove SESSION_BEGIN event handler: %d", res);
                }

                m_hasOwnProcess = false;
            }
        }

        void VoiceControl::voiceEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            VoiceControl* instance = VoiceControl::_instance;
            if (instance)
                instance->iarmEventHandler(owner, eventId, data, len);
            else
                LOGWARN("WARNING - cannot handle IARM events without a VoiceControl plugin instance!");
        }

        void VoiceControl::iarmEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            LOGINFO("Event ID %u received, data: %p, len: %u.", (unsigned)eventId, data, (unsigned)len);
            if (!strcmp(owner, CTRLM_MAIN_IARM_BUS_NAME))
            {
                if ((data == NULL) || (len <= sizeof(ctrlm_voice_iarm_event_json_t)))
                {
                    LOGERR("ERROR - got eventId(%u) with INVALID DATA: data: %p, len: %zu.", (unsigned)eventId, data, len);
                    return;
                }
                
                ctrlm_voice_iarm_event_json_t* eventData = (ctrlm_voice_iarm_event_json_t*)data;
                if (!eventData) {
                    LOGERR("ERROR - failed to cast event data: eventId: %u.", (unsigned)eventId);
                    return;
                }

                // Ensure there is a null character at the end of the data area.
                if (len > 0) {
                    char* str = (char*)data;
                    str[len - 1] = '\0';
                } else {
                    LOGERR("ERROR - invalid len value: %zu", len);
                    return;
                }

                if (CTRLM_VOICE_IARM_BUS_API_REVISION != eventData->api_revision)
                {
                    LOGERR("ERROR - got eventId(%u) with wrong VOICE IARM API revision - should be %d, event has %d.",
                           (unsigned)eventId, CTRLM_VOICE_IARM_BUS_API_REVISION, (int)eventData->api_revision);
                    return;
                }

                switch(eventId) {
                    case CTRLM_VOICE_IARM_EVENT_JSON_SESSION_BEGIN:
                        LOGWARN("Got CTRLM_VOICE_IARM_EVENT_JSON_SESSION_BEGIN event.");

                        onSessionBegin(eventData);
                        break;

                    case CTRLM_VOICE_IARM_EVENT_JSON_STREAM_BEGIN:
                        LOGWARN("Got CTRLM_VOICE_IARM_EVENT_JSON_STREAM_BEGIN event.");

                        onStreamBegin(eventData);
                        break;

                    case CTRLM_VOICE_IARM_EVENT_JSON_KEYWORD_VERIFICATION:
                        LOGWARN("Got CTRLM_VOICE_IARM_EVENT_JSON_KEYWORD_VERIFICATION event.");

                        onKeywordVerification(eventData);
                        break;

                    case CTRLM_VOICE_IARM_EVENT_JSON_SERVER_MESSAGE:
                        LOGWARN("Got CTRLM_VOICE_IARM_EVENT_JSON_SERVER_MESSAGE event.");

                        onServerMessage(eventData);
                        break;

                    case CTRLM_VOICE_IARM_EVENT_JSON_STREAM_END:
                        LOGWARN("Got CTRLM_VOICE_IARM_EVENT_JSON_STREAM_END event.");

                        onStreamEnd(eventData);
                        break;

                    case CTRLM_VOICE_IARM_EVENT_JSON_SESSION_END:
                        LOGWARN("Got CTRLM_VOICE_IARM_EVENT_JSON_SESSION_END event.");

                        onSessionEnd(eventData);
                        break;

                    default:
                        LOGERR("ERROR - unexpected ControlMgr event: eventId: %u, data: %p, size: %zu.",
                               (unsigned)eventId, data, len);
                        break;
                }
            }
            else
            {
                LOGERR("ERROR - unexpected event: owner %s, eventId: %u, data: %p, size: %zu.",
                       owner, (unsigned)eventId, data, len);
            }
        }  // End iarmEventHandler()

        //Begin methods
        uint32_t VoiceControl::getApiVersionNumber(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            response["version"] = m_apiVersionNumber;
            returnResponse(true);
        }

        void VoiceControl::getMaskPii_()
        {
            JsonObject params;
            JsonObject result;
            uint32_t retCode = voiceStatus(params, result);
            
            if (retCode == Core::ERROR_NONE && result.HasLabel("success") && result["success"].Boolean())
            {
                if (result.HasLabel("maskPii")) {
                    m_maskPii = result["maskPii"].Boolean();
                    LOGINFO("Mask pii set to %s.", (m_maskPii ? "True" : "False"));
                } else {
                    LOGWARN("voiceStatus succeeded but no maskPii field found. Defaulting to false.");
                    m_maskPii = false;
                }
            }
            else
            {
                LOGERR("voiceStatus failed with code %u. Defaulting m_maskPii to false.", retCode);
                m_maskPii = false;
            }
        }

        uint32_t VoiceControl::voiceStatus(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            IARM_Result_t                   res;
            string                          jsonParams;
            bool                            bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // Validate payload size
            if (jsonParams.size() > 100000) {
                LOGERR("ERROR - JSON payload too large: %zu bytes.", jsonParams.size());
                returnResponse(false);
            }

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            VoiceIARMCallGuard callGuard(totalsize);

            if (!callGuard.isValid())
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
                returnResponse(bSuccess);
            }

            ctrlm_voice_iarm_call_json_t* call = callGuard.get();
            // Set the call structure members appropriately.
            call->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
            size_t len = jsonParams.copy(call->payload, jsonParams.size());
            if (len != jsonParams.size()) {
                LOGERR("ERROR - Payload copy incomplete: copied %zu of %zu bytes.", len, jsonParams.size());
                returnResponse(false);
            }
            call->payload[len] = '\0';

            // Make the IARM call to controlMgr to configure the voice settings
            res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_STATUS, (void *)call, totalsize);
            if (res != IARM_RESULT_SUCCESS)
            {
                LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_STATUS Bus Call FAILED, res: %d.", (int)res);
                bSuccess = false;
            }
            else
            {
                JsonObject result;

                result.FromString(call->result);
                bSuccess = result["success"].Boolean();
                response = result;
                if(bSuccess) {
                    LOGINFO("CTRLM_VOICE_IARM_CALL_STATUS call SUCCESS!");
                } else {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_STATUS returned FAILURE!");
                }
            }

            returnResponse(bSuccess);
        }

        uint32_t VoiceControl::configureVoice(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string jsonParams;
            bool bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            VoiceIARMCallGuard call(totalsize);

            if (!call.get())
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }
            else
            {
                // Set the call structure members appropriately.
                call.get()->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call.get()->payload, jsonParams.size());
                call.get()->payload[len] = '\0';

                // Make the IARM call to controlMgr to configure the voice settings
                IARM_Result_t res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_CONFIGURE_VOICE, (void *)call.get(), totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_CONFIGURE_VOICE Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call.get()->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("CONFIGURE_VOICE call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_CONFIGURE_VOICE returned FAILURE!");
                    }
                }
            }

            returnResponse(bSuccess);
        }

        uint32_t VoiceControl::setVoiceInit(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string jsonParams;
            bool bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            VoiceIARMCallGuard call(totalsize);

            if (!call.get())
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }
            else
            {
                // Set the call structure members appropriately.
                call.get()->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call.get()->payload, jsonParams.size());
                call.get()->payload[len] = '\0';

                // Make the IARM call to controlMgr to configure the voice settings
                IARM_Result_t res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_SET_VOICE_INIT, (void *)call.get(), totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SET_VOICE_INIT Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call.get()->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("SET_VOICE_INIT call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SET_VOICE_INIT returned FAILURE!");
                    }
                }
            }

            returnResponse(bSuccess);
        }


        uint32_t VoiceControl::sendVoiceMessage(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string jsonParams;
            bool bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            VoiceIARMCallGuard call(totalsize);

            if (!call.get())
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }
            else
            {
                // Set the call structure members appropriately.
                call.get()->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call.get()->payload, jsonParams.size());
                call.get()->payload[len] = '\0';

                // Make the IARM call to controlMgr to configure the voice settings
                IARM_Result_t res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_SEND_VOICE_MESSAGE, (void *)call.get(), totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SEND_VOICE_MESSAGE Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call.get()->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("SEND_VOICE_MESSAGE call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SEND_VOICE_MESSAGE returned FAILURE!");
                    }
                }
            }

            returnResponse(bSuccess);
        }

        /**
         * @brief DEPRECATED - Use voiceSessionRequest instead
         * 
         * This method is deprecated and maintained only for backward compatibility.
         * It translates legacy "type" parameter values (ptt, ff, mic) to the newer
         * session type format (ptt_transcription, ff_transcription, mic_transcription)
         * and forwards to voiceSessionRequest.
         * 
         * @warning This function should not be used in new code. It will be removed
         * in a future API version. Please use voiceSessionRequest directly with the
         * proper session type format.
         * 
         * @param parameters Input parameters with legacy "type" field
         * @param response Output response from voiceSessionRequest
         * @return uint32_t Success or error code
         */
        uint32_t VoiceControl::voiceSessionByText(const JsonObject& parameters, JsonObject& response) // DEPRECATED
        {
           LOGWARN("voiceSessionByText is DEPRECATED - use voiceSessionRequest instead");
           
           // Translate the input parameters then call voiceSessionRequest
           JsonObject parameters_translated;

           if(!parameters.HasLabel("type")) {
              parameters_translated["type"] = "ptt_transcription";
           } else {
              std::string str_type = parameters["type"].String();
              // Case-insensitive comparison for backward compatibility
              std::transform(str_type.begin(), str_type.end(), str_type.begin(), ::tolower);
              if(str_type == "ptt") {
                 parameters_translated["type"] = "ptt_transcription";
              } else if(str_type == "ff") {
                 parameters_translated["type"] = "ff_transcription";
              } else if(str_type == "mic") {
                 parameters_translated["type"] = "mic_transcription";
              } else {
                 LOGERR("Invalid type parameter in deprecated voiceSessionByText: %s", parameters["type"].String().c_str());
                 parameters_translated["type"] = "";
              }
           }
           if(parameters.HasLabel("transcription")) {
              parameters_translated["transcription"] = parameters["transcription"];
           } // else voiceSessionRequest will return an error if transcription field is not present

           return(voiceSessionRequest(parameters_translated, response));
        }

        uint32_t VoiceControl::voiceSessionTypes(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            ctrlm_voice_iarm_call_json_t*   call = NULL;
            IARM_Result_t                   res;
            string                          jsonParams;
            bool                            bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            call = (ctrlm_voice_iarm_call_json_t*)calloc(1, totalsize);

            if (call != NULL)
            {
                // Set the call structure members appropriately.
                call->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call->payload, jsonParams.size());
                call->payload[len] = '\0';
            }
            else
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }

            if (bSuccess)
            {
                // Make the IARM call to controlMgr to configure the voice settings
                res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_SESSION_TYPES, (void *)call, totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_TYPES Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("SESSION_TYPES call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_TYPES returned FAILURE!");
                    }
                }
            }

            if (call != NULL)
            {
                free(call);
            }

            returnResponse(bSuccess);
        }

        uint32_t VoiceControl::voiceSessionRequest(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string jsonParams;
            bool bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            VoiceIARMCallGuard call(totalsize);

            if (!call.get())
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }
            else
            {
                // Set the call structure members appropriately.
                call.get()->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call.get()->payload, jsonParams.size());
                call.get()->payload[len] = '\0';

                // Make the IARM call to controlMgr to configure the voice settings
                IARM_Result_t res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_SESSION_REQUEST, (void *)call.get(), totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_REQUEST Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call.get()->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("SESSION_REQUEST call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_REQUEST returned FAILURE!");
                    }
                }
            }

            returnResponse(bSuccess);
        }

        uint32_t VoiceControl::voiceSessionTerminate(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            string jsonParams;
            bool bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            VoiceIARMCallGuard call(totalsize);

            if (!call.get())
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }
            else
            {
                // Set the call structure members appropriately.
                call.get()->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call.get()->payload, jsonParams.size());
                call.get()->payload[len] = '\0';

                // Make the IARM call to controlMgr to configure the voice settings
                IARM_Result_t res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_SESSION_TERMINATE, (void *)call.get(), totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_TERMINATE Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call.get()->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("SESSION_TERMINATE call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_TERMINATE returned FAILURE!");
                    }
                }
            }

            returnResponse(bSuccess);
        }

        uint32_t VoiceControl::voiceSessionAudioStreamStart(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();

            ctrlm_voice_iarm_call_json_t*   call = NULL;
            IARM_Result_t                   res;
            string                          jsonParams;
            bool                            bSuccess = true;

            // Just pass through the input parameters, without understanding or checking them.
            parameters.ToString(jsonParams);

            // We must allocate the memory for the call structure. Determine what we will need.
            size_t totalsize = sizeof(ctrlm_voice_iarm_call_json_t) + jsonParams.size() + 1;
            call = (ctrlm_voice_iarm_call_json_t*)calloc(1, totalsize);

            if (call != NULL)
            {
                // Set the call structure members appropriately.
                call->api_revision = CTRLM_VOICE_IARM_BUS_API_REVISION;
                size_t len = jsonParams.copy(call->payload, jsonParams.size());
                call->payload[len] = '\0';
            }
            else
            {
                LOGERR("ERROR - Cannot allocate IARM structure - size: %u.", (unsigned)totalsize);
                bSuccess = false;
            }

            if (bSuccess)
            {
                // Make the IARM call to controlMgr to start the audio stream
                res = IARM_Bus_Call(CTRLM_MAIN_IARM_BUS_NAME, CTRLM_VOICE_IARM_CALL_SESSION_AUDIO_STREAM_START, (void *)call, totalsize);
                if (res != IARM_RESULT_SUCCESS)
                {
                    LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_AUDIO_STREAM_START Bus Call FAILED, res: %d.", (int)res);
                    bSuccess = false;
                }
                else
                {
                    JsonObject result;

                    result.FromString(call->result);
                    bSuccess = result["success"].Boolean();
                    response = result;
                    if(bSuccess) {
                        LOGINFO("SESSION_AUDIO_STREAM_START call SUCCESS!");
                    } else {
                        LOGERR("ERROR - CTRLM_VOICE_IARM_CALL_SESSION_AUDIO_STREAM_START returned FAILURE!");
                    }
                }
            }

            if (call != NULL)
            {
                free(call);
            }

            returnResponse(bSuccess);
        }
        //End methods

        //Begin events
        
        // Helper method to handle voice event notifications
        void VoiceControl::handleVoiceEvent(const char* eventName, ctrlm_voice_iarm_event_json_t* eventData, bool useMaskPii)
        {
            JsonObject params;
            params.FromString(eventData->payload);
            
            if (useMaskPii) {
                sendNotify_(eventName, params);
            } else {
                sendNotify(eventName, params);
            }
        }
        
        void VoiceControl::onSessionBegin(ctrlm_voice_iarm_event_json_t* eventData)
        {
            handleVoiceEvent("onSessionBegin", eventData, false);
        }

        void VoiceControl::onStreamBegin(ctrlm_voice_iarm_event_json_t* eventData)
        {
            handleVoiceEvent("onStreamBegin", eventData, false);
        }

        void VoiceControl::onKeywordVerification(ctrlm_voice_iarm_event_json_t* eventData)
        {
            handleVoiceEvent("onKeywordVerification", eventData, false);
        }

        void VoiceControl::onServerMessage(ctrlm_voice_iarm_event_json_t* eventData)
        {
            handleVoiceEvent("onServerMessage", eventData, true);
        }

        void VoiceControl::onStreamEnd(ctrlm_voice_iarm_event_json_t* eventData)
        {
            handleVoiceEvent("onStreamEnd", eventData, false);
        }

        void VoiceControl::onSessionEnd(ctrlm_voice_iarm_event_json_t* eventData)
        {
            handleVoiceEvent("onSessionEnd", eventData, true);
        }
        //End events

        //Begin local private utility methods
        void VoiceControl::setApiVersionNumber(unsigned int apiVersionNumber)
        {
            LOGINFO("setting version: %d", (int)apiVersionNumber);
            m_apiVersionNumber = apiVersionNumber;
        }

        void VoiceControl::sendNotify_(const char* eventName, JsonObject& parameters)
        {
            // Conditional masking of PII (Personally Identifiable Information) in event notifications
            // This is intentional - when m_maskPii is true, sensitive parameters are masked before sending
            // Both branches are required for proper PII handling compliance
            if(m_maskPii)
            {
                sendNotifyMaskParameters(eventName, parameters);
            }
            else
            {
                sendNotify(eventName, parameters);
            }
        }
        //End local private utility methods

    } // namespace Plugin
} // namespace WPEFramework

