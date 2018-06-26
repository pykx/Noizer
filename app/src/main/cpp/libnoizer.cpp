#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define TAG "Noizer"
#define SIZE_BUFFER 512 * 1024

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;
static SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLEffectSendItf bqPlayerEffectSend;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

enum stateMachine {
    UNINITIALIZED,
    IS_STOPPED,
    IS_PLAYING
};

static stateMachine mState = UNINITIALIZED;
static pthread_mutex_t mStateLock = PTHREAD_MUTEX_INITIALIZER;

// aux effect on the output mix, used by the buffer queue player
static const SLEnvironmentalReverbSettings reverbSettings =
        SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

static uint8_t noiseBuffer[SIZE_BUFFER];

static bool fnGenerateNoise(uint8_t *buf, uint32_t size) {

    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Updating noise buffer ...");

    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open /dev/urandom.");
        return false;
    }

    ssize_t result = read(rng, buf, size);
    if (result != SIZE_BUFFER) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "Unsatisfied generation of pseudo-random values! Read %d to buffer, size %d",
                            result, size);
        return false;
    }

    return true;
}

// Fill buffer with noise from /dev/urandom on load
__attribute__((constructor)) static void onLoad(void) {
    fnGenerateNoise(noiseBuffer, SIZE_BUFFER);
}

// this callback handler is called every time a buffer finishes playing
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    assert(bq == bqPlayerBufferQueue);
    assert(NULL == context);

    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Callback!");

    fnGenerateNoise(noiseBuffer, SIZE_BUFFER);

    // for streaming playback, replace this test by logic to find and fill the next buffer
    SLresult result;
    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, noiseBuffer, SIZE_BUFFER);
    assert(SL_RESULT_SUCCESS == result);
}

void fnCreateBufferQueueAudioPlayer() {
    SLresult result;

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    /*
     * create audio player:
     *     fast audio does not support when SL_IID_EFFECTSEND is required, skip it
     *     for fast audio case
     */
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ };

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback,
                                                      NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the effect send interface
    bqPlayerEffectSend = NULL;
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                             &bqPlayerEffectSend);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
    // get the mute/solo interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
#endif

    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

static jboolean stop() {
    assert(mState == IS_PLAYING);

    __android_log_print(ANDROID_LOG_INFO, TAG, "Stopping noise generator ...");

    if (!pthread_mutex_lock(&mStateLock)) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        mState = IS_STOPPED;
        pthread_mutex_unlock(&mStateLock);
        return JNI_TRUE;
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to lock.");
        return JNI_FALSE;
    }
}


static jboolean play() {
    assert(mState == IS_STOPPED);

    __android_log_print(ANDROID_LOG_INFO, TAG, "Generating noise ...");

    if (!pthread_mutex_lock(&mStateLock)) {
        fnCreateBufferQueueAudioPlayer();

        fnGenerateNoise(noiseBuffer, SIZE_BUFFER);

        SLresult result;
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, noiseBuffer, SIZE_BUFFER);
        assert(result == SL_RESULT_SUCCESS);

        mState = IS_PLAYING;
        pthread_mutex_unlock(&mStateLock);

        return JNI_TRUE;
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to lock.");
        return JNI_FALSE;
    }
}


extern "C" JNICALL
void Java_com_application_noizer_NoizerService_initialize(JNIEnv *env, jclass clazz) {

    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Initializing noise generator.");

    SLresult result;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }

    // ignore unsuccessful result codes for environmental reverb, as it is optional for this example

    mState = IS_STOPPED;

    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Noise generator initialized: %d", mState);
}

extern "C" JNICALL
jboolean Java_com_application_noizer_NoizerService_play(JNIEnv *env, jclass clazz) {
    assert(mState == IS_STOPPED);
    return play();
}

extern "C" JNICALL
jboolean Java_com_application_noizer_NoizerService_stop(JNIEnv *env, jclass clazz) {
    assert(mState == IS_PLAYING);
    return stop();
}


