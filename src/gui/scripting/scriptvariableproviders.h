#pragma once

#include <core/player/playbackqueue.h>
#include <core/scripting/scriptproviders.h>

namespace Fooyin {
class PlayerController;

constexpr auto PlayingIcon = "%playingicon%";

class PlaylistScriptEnvironment : public ScriptEnvironment,
                                  public ScriptPlaylistEnvironment,
                                  public ScriptTrackListEnvironment,
                                  public ScriptPlaybackEnvironment,
                                  public ScriptEvaluationEnvironment
{
public:
    PlaylistScriptEnvironment();

    void setPlaylistData(const Playlist* playlist, const PlaylistTrackIndexes* playlistQueue,
                         const TrackList* tracks = nullptr);
    void setTrackState(int playlistTrackIndex, int currentPlayingTrackIndex, int trackDepth);
    void setPlaybackState(uint64_t currentPosition, uint64_t currentTrackDuration, int bitrate,
                          Player::PlayState playState);
    void setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder, bool escapeRichText,
                             bool useVariousArtists = false);

    [[nodiscard]] const ScriptPlaylistEnvironment* playlistEnvironment() const override;
    [[nodiscard]] const ScriptTrackListEnvironment* trackListEnvironment() const override;
    [[nodiscard]] const ScriptPlaybackEnvironment* playbackEnvironment() const override;
    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override;

    [[nodiscard]] int currentPlaylistTrackIndex() const override;
    [[nodiscard]] int currentPlayingTrackIndex() const override;
    [[nodiscard]] int playlistTrackCount() const override;
    [[nodiscard]] int trackDepth() const override;
    [[nodiscard]] std::span<const int> currentQueueIndexes() const override;
    [[nodiscard]] const TrackList* trackList() const override;
    [[nodiscard]] uint64_t currentPosition() const override;
    [[nodiscard]] uint64_t currentTrackDuration() const override;
    [[nodiscard]] int bitrate() const override;
    [[nodiscard]] Player::PlayState playState() const override;
    [[nodiscard]] TrackListContextPolicy trackListContextPolicy() const override;
    [[nodiscard]] QString trackListPlaceholder() const override;
    [[nodiscard]] bool escapeRichText() const override;
    [[nodiscard]] bool useVariousArtists() const override;

private:
    const Playlist* m_playlist;
    const PlaylistTrackIndexes* m_playlistQueue;
    const TrackList* m_tracks;
    int m_playlistTrackIndex;
    int m_currentPlayingTrackIndex;
    int m_trackDepth;
    uint64_t m_currentPosition;
    uint64_t m_currentTrackDuration;
    int m_bitrate;
    Player::PlayState m_playState;
    TrackListContextPolicy m_trackListContextPolicy;
    QString m_trackListPlaceholder;
    bool m_escapeRichText;
    bool m_useVariousArtists;
};

struct PlaybackScriptContextData
{
    PlaylistTrackIndexes playlistQueue;
    TrackList tracks;
    PlaylistScriptEnvironment environment;
    ScriptContext context;

    PlaybackScriptContextData();
    PlaybackScriptContextData(const PlaybackScriptContextData&)            = delete;
    PlaybackScriptContextData& operator=(const PlaybackScriptContextData&) = delete;
    PlaybackScriptContextData(PlaybackScriptContextData&& other) noexcept;
    PlaybackScriptContextData& operator=(PlaybackScriptContextData&& other) noexcept;
};

[[nodiscard]] const ScriptVariableProvider& artworkMarkerVariableProvider();
[[nodiscard]] const ScriptVariableProvider& playlistVariableProvider();
[[nodiscard]] PlaybackScriptContextData makePlaybackScriptContext(PlayerController* playerController,
                                                                  Playlist* playlist, TrackListContextPolicy policy,
                                                                  QString placeholder = {}, bool escapeRichText = false,
                                                                  bool useVariousArtists = false);
} // namespace Fooyin
