#ifdef _DEBUG
#include <stdio.h>
#endif

#include "../bass/bassmix.h"
#include "crossfade_config.h"
#include "crossfade_envelope.h"
#include "crossfade_mixer.h"
#include "crossfade_queue.h"
#include "crossfade_syncs.h"

static void CALLBACK __crossfade_mixer_free(HSYNC handle, DWORD channel, DWORD data, void* user) {
	crossfade_config_set(CF_MIXER, 0);
}

BOOL crossfade_mixer_get(HSTREAM* const handle) {
	return crossfade_config_get(CF_MIXER, handle);
}

HSTREAM crossfade_mixer_create(const DWORD freq, const DWORD chans, const DWORD flags, const void* const user) {
	HSTREAM mixer = BASS_Mixer_StreamCreate(freq, chans, flags);
	if (!mixer) {
		return 0;
	}
	BASS_ChannelSetSync(mixer, BASS_SYNC_FREE, 0, &__crossfade_mixer_free, NULL);
	crossfade_config_set(CF_MIXER, mixer);
	return mixer;
}

HSTREAM* crossfade_mixer_get_all(DWORD* const count) {
	HSTREAM mixer;
	static HSTREAM handles[MAX_CHANNELS] = { 0 };
	if (!crossfade_mixer_get(&mixer)) {
		*count = 0;
		return NULL;
	}
	*count = BASS_Mixer_StreamGetChannels(mixer, handles, MAX_CHANNELS);
	return handles;
}

BOOL crossfade_mixer_peek(HSTREAM* const handle) {
	DWORD count;
	HSTREAM* handles = crossfade_mixer_get_all(&count);
	if (count == 0) {
		return FALSE;
	}
	*handle = handles[0];
	return TRUE;
}

BOOL crossfade_mixer_playing() {
	DWORD position;
	DWORD count;
	HSTREAM* handles = crossfade_mixer_get_all(&count);
	for (position = 0; position < count; position++) {
		if (BASS_Mixer_ChannelGetMixer(handles[position]) && BASS_ChannelIsActive(handles[position]) == BASS_ACTIVE_PLAYING) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL crossfade_mixer_wait(const HSTREAM handle, const DWORD period) {
	//Wait until the channel is;
	//a) Removed from the mixer (probably by BASS_MIXER_ENV_REMOVE).
	//b) Is no longer active.
	//c) Stops decoding.
	QWORD last_position = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);
	QWORD current_position;
	Sleep(period);
	while (BASS_Mixer_ChannelGetMixer(handle) && BASS_ChannelIsActive(handle) == BASS_ACTIVE_PLAYING) {
		Sleep(10);
		current_position = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);
		if (current_position == last_position) {
			return FALSE;
		}
		last_position = current_position;
	}
	return TRUE;
}

BOOL crossfade_mixer_add(const HSTREAM handle, const BOOL fade_in) {
	DWORD period;
	BOOL buffer;
	HSTREAM mixer;
	DWORD flags = BASS_MIXER_CHAN_PAUSE | BASS_MIXER_NORAMPIN;
	if (!crossfade_mixer_get(&mixer)) {
		return FALSE;
	}
	crossfade_sync_register(handle);
	crossfade_config_get(CF_BUFFER, &buffer);
	if (buffer) {
		flags |= BASS_MIXER_BUFFER;
	}
	if (fade_in) {
		crossfade_config_get(CF_OUT_PERIOD, &period);
		if (period) {
			if (!BASS_Mixer_StreamAddChannel(mixer, handle, flags)) {
				return FALSE;
			}
			crossfade_envelope_apply_in(handle);
			BASS_Mixer_ChannelFlags(handle, 0, BASS_MIXER_CHAN_PAUSE);
			return TRUE;
		}
	}
	return BASS_Mixer_StreamAddChannel(mixer, handle, BASS_MIXER_NORAMPIN);
}

BOOL crossfade_mixer_remove(const HSTREAM handle, const BOOL fade_out) {
	DWORD period;
	crossfade_sync_unregister(handle);
	if (fade_out) {
		if (BASS_Mixer_ChannelGetMixer(handle) && BASS_ChannelIsActive(handle) == BASS_ACTIVE_PLAYING) {
			crossfade_config_get(CF_OUT_PERIOD, &period);
			if (period) {
				crossfade_envelope_apply_out(handle, TRUE);
				return crossfade_mixer_wait(handle, period);
			}
		}
	}
	return BASS_Mixer_ChannelRemove(handle);
}

BOOL crossfade_mixer_next(const BOOL fade_in) {
	HSTREAM mixer;
	HSTREAM handle;
	if (!crossfade_mixer_get(&mixer)) {
		return FALSE;
	}
	if (!crossfade_queue_peek(&handle)) {
		return FALSE;
	}
	if (!crossfade_mixer_add(handle, fade_in)) {
		return FALSE;
	}
	return crossfade_queue_remove(handle);
}