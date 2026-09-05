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

#include "Constants.h"
#include "SubsonicIndexerSource.h"
#include "SubsonicClient.h"

#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IPreferences.h>

#include <functional>

using namespace musik::core::sdk;
using namespace musik::core::subsonic;

extern IDebug* debug;
extern IPreferences* prefs;

SubsonicIndexerSource::SubsonicIndexerSource() {
}

SubsonicIndexerSource::~SubsonicIndexerSource() {
}

void SubsonicIndexerSource::Release() {
    delete this;
}

void SubsonicIndexerSource::OnBeforeScan() {
    this->interrupted = false;
}

void SubsonicIndexerSource::OnAfterScan() {
}

void SubsonicIndexerSource::Interrupt() {
    this->interrupted = true;
}

int SubsonicIndexerSource::SourceId() {
    return std::hash<std::string>()(PLUGIN_NAME);
}

ScanResult SubsonicIndexerSource::Scan(
    IIndexerWriter* indexer,
    const char** /* indexerPaths */,
    unsigned /* indexerPathsCount */)
{
    this->knownExternalIds.clear();
    this->lastScanSucceeded = false;

    if (!prefs || !prefs->GetBool(KEY_ENABLED, false)) {
        return ScanCommit;
    }

    const std::string hostname = getPreferenceString<std::string>(prefs, KEY_HOSTNAME, "");
    const std::string username = getPreferenceString<std::string>(prefs, KEY_USERNAME, "");
    const std::string password = getPreferenceString<std::string>(prefs, KEY_PASSWORD, "");

    SubsonicClient client(hostname, username, password);

    if (!client.IsConfigured()) {
        if (debug) {
            debug->Warning(PLUGIN_NAME, "hostname, username, or password not configured; skipping scan");
        }
        return ScanCommit;
    }

    std::string error;
    if (!client.Ping(error)) {
        if (debug) {
            debug->Error(PLUGIN_NAME, ("failed to connect to server: " + error).c_str());
        }
        /* leave existing tracks alone on a transient connection failure --
        don't let ScanTrack() remove everything just because the server was
        briefly unreachable. */
        return ScanCommit;
    }

    std::vector<Artist> artists;
    if (!client.GetArtists(artists, error)) {
        if (debug) {
            debug->Error(PLUGIN_NAME, ("failed to fetch artists: " + error).c_str());
        }
        return ScanCommit;
    }

    size_t tracksIndexed = 0;
    size_t lastCommittedCount = 0;

    /* Navidrome (and other Subsonic servers) expose a separate ID3 "artist"
    entry for each distinct credit variant of a collaboration (e.g. "100 gecs"
    and "100 gecs feat. Skrillex" are different artist ids). Both variants'
    discographies can reference the same underlying albums/tracks, so we
    dedupe at both levels to avoid redundant HTTP round-trips and inflated
    progress counts. */
    std::set<std::string> seenAlbumIds;

    size_t artistIndex = 0;

    for (auto& artist : artists) {
        if (this->interrupted) {
            break;
        }

        if (debug) {
            debug->Info(PLUGIN_NAME, ("[debug] fetching albums for artist " +
                std::to_string(++artistIndex) + "/" + std::to_string(artists.size()) +
                " '" + artist.name + "' (id=" + artist.id + ")").c_str());
        }

        std::vector<Album> albums;
        if (!client.GetAlbumsForArtist(artist.id, albums, error)) {
            if (debug) {
                debug->Warning(PLUGIN_NAME, ("failed to fetch albums for artist '" + artist.name + "': " + error).c_str());
            }
            continue;
        }

        for (auto& album : albums) {
            if (this->interrupted) {
                break;
            }

            if (!album.id.size() || !seenAlbumIds.insert(album.id).second) {
                continue;
            }

            if (debug) {
                debug->Info(PLUGIN_NAME, ("[debug] fetching tracks for album '" +
                    album.name + "' (id=" + album.id + ")").c_str());
            }

            std::vector<Track> tracks;
            if (!client.GetTracksForAlbum(album.id, tracks, error)) {
                if (debug) {
                    debug->Warning(PLUGIN_NAME, ("failed to fetch tracks for album '" + album.name + "': " + error).c_str());
                }
                continue;
            }

            for (auto& track : tracks) {
                if (!track.id.size()) {
                    continue;
                }

                if (!this->knownExternalIds.insert(track.id).second) {
                    continue;
                }

                ITagStore* writer = indexer->CreateWriter();
                writer->SetValue("title", track.title.c_str());
                writer->SetValue("album", track.album.c_str());
                writer->SetValue("artist", track.artist.c_str());
                writer->SetValue("album_artist", track.albumArtist.c_str());
                writer->SetValue("genre", track.genre.c_str());
                writer->SetValue("track", track.track.c_str());
                writer->SetValue("disc", track.disc.c_str());
                writer->SetValue("duration", track.duration.c_str());
                writer->SetValue("filename", client.GetStreamUrl(track.id).c_str());

                indexer->Save(this, writer, track.id.c_str());
                writer->Release();

                if (++tracksIndexed % 100 == 0) {
                    /* CommitProgress()/IncrementTracksScanned() add this value
                    to a running total -- it's a delta since the last call, not
                    a cumulative count. */
                    indexer->CommitProgress(this, (unsigned int) (tracksIndexed - lastCommittedCount));
                    lastCommittedCount = tracksIndexed;
                }
            }
        }
    }

    indexer->CommitProgress(this, (unsigned int) (tracksIndexed - lastCommittedCount));

    this->lastScanSucceeded = !this->interrupted;

    return ScanCommit;
}

void SubsonicIndexerSource::ScanTrack(
    IIndexerWriter* indexer,
    ITagStore* /* tagStore */,
    const char* externalId)
{
    /* only remove tracks that have disappeared server-side if we were
    actually able to complete a full scan this pass -- otherwise a transient
    network failure would wipe out the whole library. */
    if (!this->lastScanSucceeded) {
        return;
    }

    if (this->knownExternalIds.find(externalId) == this->knownExternalIds.end()) {
        indexer->RemoveByExternalId(this, externalId);
    }
}
