/*
 * Copyright (C) 2019 Haiku, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

enum {
    DID_COMMIT_NAVIGATION = 'dcna',
    DID_FINISH_NAVIGATION = 'dfna',
    URL_CHANGE = 'urlc',
    DID_CHANGE_PROGRESS = 'dcpr',
    DID_CHANGE_TITLE = 'dctt',
    URL_LOAD_HANDLE = 'urlh',
    READY_TO_PAINT = 'retp',

    // NavigationClient events
    DID_START_PROVISIONAL_NAVIGATION = 'dspn',
    DID_RECEIVE_SERVER_REDIRECT_FOR_PROVISIONAL_NAVIGATION = 'dsrp',
    DID_FAIL_PROVISIONAL_NAVIGATION = 'dfpn',
    DID_FAIL_NAVIGATION = 'dfin',
    DID_SAME_DOCUMENT_NAVIGATION = 'dsdn',
    RENDERING_PROGRESS_DID_CHANGE = 'rpdc',

    // PageLoadStateObserver events
    DID_CHANGE_BACK_FORWARD = 'dcbf',
    DID_CHANGE_IS_LOADING = 'dcil',
    DID_CHANGE_ACTIVE_URL = 'dcau',
    DID_CHANGE_NETWORK_REQUESTS = 'dcnr',

    SHOW_CERTIFICATE_INFO = 'shci',

    // PageUIClient events
    CREATE_NEW_PAGE = 'crnp',
    SHOW_PAGE = 'shpg',
    CLOSE_PAGE = 'clpg',
    RUN_JAVASCRIPT_ALERT = 'rjka',
    RUN_JAVASCRIPT_CONFIRM = 'rjkc',
    RUN_JAVASCRIPT_PROMPT = 'rjkp',
    SET_STATUS_TEXT = 'stst',
    MOUSE_DID_MOVE_OVER_ELEMENT = 'mdmo',

    TOOLBARS_VISIBILITY_CHANGED = 'tvch',
    MENU_BAR_VISIBILITY_CHANGED = 'mbvc',
    STATUS_BAR_VISIBILITY_CHANGED = 'sbvc',
    RESIZABLE_CHANGED = 'rsch',
    WINDOW_FRAME_CHANGED = 'wfch',

    // PageClient events
    CONTENT_SIZE_CHANGED = 'csch',
    PROCESS_DID_EXIT = 'prde',
    PROCESS_DID_RELAUNCH = 'prdr',
    PAGE_CLOSED = 'pgcl',
    IS_PLAYING_AUDIO_CHANGED = 'ipac'
};
