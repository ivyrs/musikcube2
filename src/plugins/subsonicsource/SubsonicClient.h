//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>

namespace musik { namespace core { namespace subsonic {

    struct Artist {
        std::string id;
        std::string name;
    };

    struct Album {
        std::string id;
        std::string name;
    };

    struct Track {
        std::string id;
        std::string title;
        std::string album;
        std::string artist;
        std::string albumArtist;
        std::string genre;
        std::string track;
        std::string disc;
        std::string duration; /* seconds, as a string, matches ITagStore::SetValue convention */
    };

    /* thin synchronous client for the subset of the Subsonic API (ID3-oriented
    browsing + streaming) that Navidrome supports. calls block the calling
    thread -- callers are expected to invoke this from a background indexer
    thread, not the UI thread. */
    class SubsonicClient {
        public:
            SubsonicClient(
                const std::string& hostname,
                const std::string& username,
                const std::string& password);

            /* returns true if the server responded with status "ok" */
            bool Ping(std::string& errorMessage);

            bool GetArtists(std::vector<Artist>& artists, std::string& errorMessage);
            bool GetAlbumsForArtist(const std::string& artistId, std::vector<Album>& albums, std::string& errorMessage);
            bool GetTracksForAlbum(const std::string& albumId, std::vector<Track>& tracks, std::string& errorMessage);

            /* fully-formed, authenticated URL suitable for direct HTTP(S)
            streaming playback */
            std::string GetStreamUrl(const std::string& trackId) const;

            bool IsConfigured() const;

        private:
            std::string BuildUrl(const std::string& endpoint, const std::string& extraParams = "") const;
            bool Get(const std::string& endpoint, const std::string& extraParams, std::string& body, std::string& errorMessage);

            std::string hostname;
            std::string username;
            std::string password;
    };

} } }
