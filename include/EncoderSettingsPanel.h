#ifndef ENCODER_SETTINGS_PANEL_H
#define ENCODER_SETTINGS_PANEL_H

#include <SDL3/SDL.h>
#include <string>
#include "EncoderSettings.h"

class EncoderSettingsPanel {
public:
    enum class Result { None, Cancel, Export, BrowseSourceFile, BrowseFile };

    ~EncoderSettingsPanel();

    void   render(SDL_Renderer* renderer, float W, float H);
    Result handleMouseClick(float mx, float my);
    void   handleMouseMotion(float mx, float my);
    void   handleMouseButtonUp(float mx, float my);
    void   setVideoDuration(double d);
    void   setSourceFilePath(const std::string& filePath);
    void   setOutputFilePath(const std::string& filePath);
    void   handleTextInput(const char* text);
    void   handleKeyDown(SDL_Keycode key);

    EncoderSettings getSettings() const;

private:
    int   m_codecIdx   = 0; // 0=H264, 1=MPEG4
    int   m_fmtIdx     = 0; // 0=MP4,  1=TS
    int   m_videoBrIdx = 2; // index into VIDEO_BITRATES[] — default 2000 kbps
    int   m_audioBrIdx = 1; // index into AUDIO_BITRATES[] — default 128 kbps

    double m_videoDuration = 0.0;
    float  m_startValue    = 0.0f;
    float  m_endValue      = 0.0f;
    bool   m_draggingStart = false;
    bool   m_draggingEnd   = false;

    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;

    SDL_FRect m_codecPrevRect   = {};
    SDL_FRect m_codecNextRect   = {};
    SDL_FRect m_fmtPrevRect     = {};
    SDL_FRect m_fmtNextRect     = {};
    SDL_FRect m_videoBrPrevRect = {};
    SDL_FRect m_videoBrNextRect = {};
    SDL_FRect m_audioBrPrevRect = {};
    SDL_FRect m_audioBrNextRect = {};
    SDL_FRect   m_rangeTrack       = {};
    SDL_FRect   m_startThumb       = {};
    SDL_FRect   m_endThumb         = {};
    SDL_FRect   m_sourceFieldRect   = {};
    SDL_FRect   m_sourceBrowseRect  = {};
    SDL_FRect   m_fileFieldRect     = {};
    SDL_FRect   m_outputBrowseRect  = {};
    SDL_FRect   m_cancelRect        = {};
    SDL_FRect   m_exportRect        = {};

    SDL_Texture* m_browseIconTex    = nullptr;

    std::string m_sourceFilePath;
    bool        m_sourceFocused     = false;
    std::string m_filePath;
    bool        m_fileFocused       = false;
};

#endif // ENCODER_SETTINGS_PANEL_H
