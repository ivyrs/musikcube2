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

#include "SubsonicClient.h"
#include "Md5.h"

#include <musikcore/sdk/HttpClient.h>

#include <sstream>
#include <random>
#include <cstring>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#pragma warning(pop)

using Client = musik::core::sdk::HttpClient<std::stringstream>;

static const std::string API_VERSION = "1.16.1";
static const std::string CLIENT_NAME = "musikcube";

namespace musik { namespace core { namespace subsonic {

    static std::string urlEncode(const std::string& value) {
        static CURL* curl = curl_easy_init();
        if (curl && value.size()) {
            char* encoded = curl_easy_escape(curl, value.c_str(), (int) value.size());
            if (encoded) {
                std::string result(encoded);
                curl_free(encoded);
                return result;
            }
        }
        return value;
    }

    static std::string randomSalt() {
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, (int) sizeof(alphabet) - 2);
        std::string salt;
        for (int i = 0; i < 12; i++) {
            salt.push_back(alphabet[dist(gen)]);
        }
        return salt;
    }

    static std::string normalizeHostname(std::string hostname) {
        while (hostname.size() && hostname.back() == '/') {
            hostname.pop_back();
        }
        if (hostname.find("://") == std::string::npos) {
            hostname = "http://" + hostname;
        }
        return hostname;
    }

    SubsonicClient::SubsonicClient(
        const std::string& hostname,
        const std::string& username,
        const std::string& password)
    : hostname(normalizeHostname(hostname))
    , username(username)
    , password(password) {
    }

    bool SubsonicClient::IsConfigured() const {
        return this->hostname.size() > strlen("http://") &&
            this->username.size() > 0 &&
            this->password.size() > 0;
    }

    std::string SubsonicClient::BuildUrl(const std::string& endpoint, const std::string& extraParams) const {
        const std::string salt = randomSalt();
        const std::string token = Md5Hex(this->password + salt);

        std::string url = this->hostname + "/rest/" + endpoint + "?";
        url += "u=" + urlEncode(this->username);
        url += "&t=" + token;
        url += "&s=" + salt;
        url += "&v=" + API_VERSION;
        url += "&c=" + CLIENT_NAME;
        url += "&f=json";

        if (extraParams.size()) {
            url += "&" + extraParams;
        }

        return url;
    }

    bool SubsonicClient::Get(
        const std::string& endpoint,
        const std::string& extraParams,
        std::string& body,
        std::string& errorMessage)
    {
        const std::string url = this->BuildUrl(endpoint, extraParams);

        auto client = Client::Create(std::stringstream());

        long httpStatus = 0;
        CURLcode curlCode = CURLE_OK;

        client->Url(url)
            .Mode(Client::Thread::Current)
            .Run([&](Client* caller, int status, CURLcode code) {
                httpStatus = status;
                curlCode = code;
                body = caller->Stream().str();
            });

        if (curlCode != CURLE_OK || httpStatus < 200 || httpStatus >= 300) {
            errorMessage = "http request failed (status=" +
                std::to_string(httpStatus) + ", curl=" + std::to_string((int) curlCode) + ")";
            return false;
        }

        return true;
    }

    /* strips a trailing "(...)"/"[...]" group, e.g. "Album (White Vinyl)" ->
    "Album". Many Subsonic servers (Navidrome included) fold pressing/edition
    metadata straight into the album title; most listeners just want the
    album name. Mirrors navitu's subsonic::strip_edition_suffix() so both
    clients treat this the same way. */
    static std::string stripEditionSuffix(const std::string& name) {
        size_t end = name.find_last_not_of(" \t\r\n");
        if (end == std::string::npos) {
            return name;
        }
        std::string trimmed = name.substr(0, end + 1);

        char open = 0;
        if (trimmed.back() == ')') open = '(';
        else if (trimmed.back() == ']') open = '[';

        if (open) {
            size_t openPos = trimmed.rfind(open);
            if (openPos != std::string::npos) {
                std::string prefix = trimmed.substr(0, openPos);
                size_t prefixEnd = prefix.find_last_not_of(" \t\r\n");
                return prefixEnd == std::string::npos ? "" : prefix.substr(0, prefixEnd + 1);
            }
        }

        return trimmed;
    }

    static bool checkResponse(const nlohmann::json& root, std::string& errorMessage) {
        try {
            const auto& response = root.at("subsonic-response");
            const std::string status = response.value("status", "");
            if (status != "ok") {
                const auto& error = response.value("error", nlohmann::json::object());
                errorMessage = "subsonic error " +
                    std::to_string(error.value("code", 0)) + ": " +
                    error.value("message", "unknown error");
                return false;
            }
            return true;
        }
        catch (std::exception& ex) {
            errorMessage = std::string("failed to parse subsonic response: ") + ex.what();
            return false;
        }
    }

    bool SubsonicClient::Ping(std::string& errorMessage) {
        std::string body;
        if (!this->Get("ping.view", "", body, errorMessage)) {
            return false;
        }

        try {
            auto root = nlohmann::json::parse(body);
            return checkResponse(root, errorMessage);
        }
        catch (std::exception& ex) {
            errorMessage = std::string("failed to parse subsonic response: ") + ex.what();
            return false;
        }
    }

    bool SubsonicClient::GetArtists(std::vector<Artist>& artists, std::string& errorMessage) {
        std::string body;
        if (!this->Get("getArtists.view", "", body, errorMessage)) {
            return false;
        }

        try {
            auto root = nlohmann::json::parse(body);
            if (!checkResponse(root, errorMessage)) {
                return false;
            }

            const auto& indexes = root["subsonic-response"]["artists"].value("index", nlohmann::json::array());
            for (const auto& index : indexes) {
                const auto& entries = index.value("artist", nlohmann::json::array());
                for (const auto& entry : entries) {
                    Artist artist;
                    artist.id = entry.value("id", "");
                    artist.name = entry.value("name", "");
                    if (artist.id.size()) {
                        artists.push_back(artist);
                    }
                }
            }

            return true;
        }
        catch (std::exception& ex) {
            errorMessage = std::string("failed to parse getArtists response: ") + ex.what();
            return false;
        }
    }

    bool SubsonicClient::GetAlbumsForArtist(
        const std::string& artistId, std::vector<Album>& albums, std::string& errorMessage)
    {
        std::string body;
        if (!this->Get("getArtist.view", "id=" + urlEncode(artistId), body, errorMessage)) {
            return false;
        }

        try {
            auto root = nlohmann::json::parse(body);
            if (!checkResponse(root, errorMessage)) {
                return false;
            }

            const auto& entries = root["subsonic-response"]["artist"].value("album", nlohmann::json::array());
            for (const auto& entry : entries) {
                Album album;
                album.id = entry.value("id", "");
                album.name = entry.value("name", entry.value("title", ""));
                if (album.id.size()) {
                    albums.push_back(album);
                }
            }

            return true;
        }
        catch (std::exception& ex) {
            errorMessage = std::string("failed to parse getArtist response: ") + ex.what();
            return false;
        }
    }

    bool SubsonicClient::GetTracksForAlbum(
        const std::string& albumId, std::vector<Track>& tracks, std::string& errorMessage)
    {
        std::string body;
        if (!this->Get("getAlbum.view", "id=" + urlEncode(albumId), body, errorMessage)) {
            return false;
        }

        try {
            auto root = nlohmann::json::parse(body);
            if (!checkResponse(root, errorMessage)) {
                return false;
            }

            const auto& entries = root["subsonic-response"]["album"].value("song", nlohmann::json::array());
            for (const auto& entry : entries) {
                Track track;
                track.id = entry.value("id", "");
                track.title = entry.value("title", "");
                track.album = stripEditionSuffix(entry.value("album", ""));
                track.artist = entry.value("artist", "");
                /* the flat "artist" field legitimately includes feature
                credits (e.g. "100 gecs feat. Skrillex") straight from the
                file's tags -- that's fine for the track artist. But there's
                no "albumArtist" field in Navidrome's response (that key
                never exists, so this always silently fell back to `artist`,
                polluting album grouping with per-track feature credits).
                "displayAlbumArtist" is Navidrome's purpose-built clean album
                artist string for exactly this case. */
                track.albumArtist = entry.value("displayAlbumArtist", track.artist);
                track.genre = entry.value("genre", "");
                track.track = std::to_string(entry.value("track", 0));
                track.disc = std::to_string(entry.value("discNumber", 1));
                track.duration = std::to_string(entry.value("duration", 0));

                if (track.id.size()) {
                    tracks.push_back(track);
                }
            }

            return true;
        }
        catch (std::exception& ex) {
            errorMessage = std::string("failed to parse getAlbum response: ") + ex.what();
            return false;
        }
    }

    std::string SubsonicClient::GetStreamUrl(const std::string& trackId) const {
        return this->BuildUrl("stream.view", "id=" + urlEncode(trackId));
    }

} } }
