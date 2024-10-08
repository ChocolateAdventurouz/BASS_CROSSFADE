#ifdef _DEBUG
#include <stdio.h>
#endif

#include "crossfade_config.h"
#include "crossfade_envelope.h"
#include "crossfade_mixer.h"
#include "crossfade_syncs.h"

typedef struct {
	HSTREAM handle;
	HSYNC next;
	HSYNC mix;
	HSYNC fade_in;
	HSYNC fade_out;
	HSYNC unregister;
} CF_SYNCS;

static CF_SYNCS syncs[MAX_CHANNELS] = { 0 };

static void CALLBACK __crossfade_sync_next(HSYNC handle, DWORD channel, DWORD data, void* user) {
	BOOL success = TRUE;
	success &= crossfade_mixer_next(FALSE);
	success &= crossfade_mixer_remove(channel, FALSE);
#if _DEBUG
	if (!success) {
		printf("__crossfade_sync_next: Callback failed.\n");
	}
#endif
}

static void CALLBACK __crossfade_sync_mix(HSYNC handle, DWORD channel, DWORD data, void* user) {
	BOOL success = TRUE;
	success &= crossfade_mixer_next(TRUE);
	success &= crossfade_envelope_apply_out(channel, FALSE);
#if _DEBUG
	if (!success) {
		printf("__crossfade_sync_mix: Callback failed.\n");
	}
#endif
}

static void CALLBACK __crossfade_sync_fade_in(HSYNC handle, DWORD channel, DWORD data, void* user) {
	BOOL success = TRUE;
	success &= crossfade_mixer_next(TRUE);
#if _DEBUG
	if (!success) {
		printf("__crossfade_sync_fade_in: Callback failed.\n");
	}
#endif
}

static void CALLBACK __crossfade_sync_fade_out(HSYNC handle, DWORD channel, DWORD data, void* user) {
	BOOL success = TRUE;
	success &= crossfade_envelope_apply_out(channel, FALSE);
#if _DEBUG
	if (!success) {
		printf("__crossfade_sync_fade_out: Callback failed.\n");
	}
#endif
}

static void CALLBACK __crossfade_sync_unregister(HSYNC handle, DWORD channel, DWORD data, void* user) {
	BOOL success = TRUE;
	success &= crossfade_sync_unregister(channel);
#if _DEBUG
	if (!success) {
		printf("__crossfade_sync_unregister: Callback failed.\n");
	}
#endif
}

BOOL crossfade_sync_register(const HSTREAM handle) {
	DWORD position;
	DWORD mode;
	DWORD period;
	DWORD mix;
	crossfade_config_get(CF_MODE, &mode);
	crossfade_config_get(CF_OUT_PERIOD, &period);
	crossfade_config_get(CF_MIX, &mix);
	for (position = 0; position < MAX_CHANNELS; position++) {
		if (syncs[position].handle) {
			continue;
		}
		syncs[position].handle = handle;
		if (mode == CF_ALWAYS && period > 0) {
			if (mix) {
				syncs[position].mix = BASS_ChannelSetSync(
					handle,
					BASS_SYNC_POS,
					BASS_ChannelGetLength(handle, BASS_POS_BYTE) - BASS_ChannelSeconds2Bytes(handle, period / 1000),
					&__crossfade_sync_mix,
					NULL
				);
			}
			else {
				syncs[position].fade_in = BASS_ChannelSetSync(
					handle,
					BASS_SYNC_END,
					0,
					&__crossfade_sync_fade_in,
					NULL
				);
				syncs[position].fade_out = BASS_ChannelSetSync(
					handle,
					BASS_SYNC_POS,
					BASS_ChannelGetLength(handle, BASS_POS_BYTE) - BASS_ChannelSeconds2Bytes(handle, period / 1000),
					&__crossfade_sync_fade_out,
					NULL
				);
			}
		}
		else {
			syncs[position].next = BASS_ChannelSetSync(
				handle,
				BASS_SYNC_END,
				0,
				&__crossfade_sync_next,
				NULL
			);
		}
		syncs[position].unregister = BASS_ChannelSetSync(
			handle,
			BASS_SYNC_FREE,
			0,
			&__crossfade_sync_unregister,
			NULL
		);
		return TRUE;
	}
	return FALSE;
}

BOOL crossfade_sync_unregister(const HSTREAM handle) {
	DWORD position;
	for (position = 0; position < MAX_CHANNELS; position++) {
		if (syncs[position].handle != handle) {
			continue;
		}
		if (syncs[position].next) {
			BASS_ChannelRemoveSync(handle, syncs[position].next);
		}
		if (syncs[position].mix) {
			BASS_ChannelRemoveSync(handle, syncs[position].mix);
		}
		if (syncs[position].fade_in) {
			BASS_ChannelRemoveSync(handle, syncs[position].fade_in);
		}
		if (syncs[position].fade_out) {
			BASS_ChannelRemoveSync(handle, syncs[position].fade_out);
		}
		if (syncs[position].unregister) {
			BASS_ChannelRemoveSync(handle, syncs[position].unregister);
		}
		syncs[position].handle = 0;
		syncs[position].next = 0;
		syncs[position].mix = 0;
		syncs[position].fade_in = 0;
		syncs[position].fade_out = 0;
		syncs[position].unregister = 0;
		return TRUE;
	}
	return FALSE;
}

BOOL crossfade_sync_refresh() {
	DWORD position;
	DWORD handle;
	BOOL success = TRUE;
	for (position = 0; position < MAX_CHANNELS; position++) {
		handle = syncs[position].handle;
		if (!handle) {
			continue;
		}
		crossfade_sync_unregister(handle);
		crossfade_sync_register(handle);
	}
	return success;
}