#ifndef FPSPARTY_SCENE_TIMELINE_HPP
#define FPSPARTY_SCENE_TIMELINE_HPP

#include <bit>
#include <cassert>

#include "keyframe.hpp"

namespace fpsparty::scene {

struct Scene_create_info {
  float keyframe_duration;
};

// TODO: clean up API so Scene is constructed with an initial keyframe
class Scene {
public:
  explicit Scene(Scene_create_info const &info);

  /*
   * Appends a keyframe to the timeline.
   *
   * After the first call, keyframe must have a frame number greater than any
   * previous keyframe. Otherwise is UB.
   */
  void push(Keyframe &&keyframe);

  /*
   * Advances the timeline by the given duration.
   *
   * Returns true while playback is not starved.
   *
   * UB if empty.
   */
  bool play(float duration);

  /**
   * Fast-forwards the timeline to the given latency.
   *
   * If latency is behind curent playback, has no effect.
   *
   * If latency is ahead of most recrently pushed keyframe, clamps.
   *
   * UB if empty.
   *
   * Returns latency after the effect of the call.
   */
  float set_latency(float seconds) noexcept;

  /**
   * Returns the latency (positive) between the playback point and the
   * most recently pushed keyframe.
   *
   * UB if empty.
   */
  float get_latency() const noexcept;

  /*
   * Returns the grid state at the current frame.
   */
  game::Grid const &get_grid() const noexcept;

  bool get_grid_remesh_flag() const noexcept;

  void reset_grid_remesh_flag() noexcept;

  /*
   * Returns the cached interpolated cameras from the last advance_time call.
   *
   * UB if empty.
   */
  std::span<Identified<Camera> const> get_interpolated_cameras() const noexcept;

  /**
   * Returns the cached interpolated cameras from the last advance_time call.
   *
   * UB if empty.
   */
  Camera const *get_interpolated_camera(std::uint64_t id) const noexcept;

  /*
   * Returns the cached interpolated mesh instances from the last advance_time
   * call.
   *
   * UB if empty.
   */
  std::span<Identified<Mesh_instance> const>
  get_interpolated_mesh_instances() const noexcept;

  /*
   * Returns the number of keyframes stored.
   */
  std::size_t get_keyframe_count() const noexcept;

  /*
   * Returns the base frame number of the playback point.
   */
  std::uint64_t get_keyframe_number() const noexcept;

  /*
   * Returns the inter-keyframe time of the playback point.
   * This is on a scale of [0, 1).
   */
  float get_inter_keyframe_time() const noexcept;

  /*
   * Returns the duration of a single keyframe.
   * Passed in the constructor.
   */
  float get_keyframe_duration() const noexcept;

  /*
   * True until the first keyframe is pushed.
   */
  bool empty() const noexcept;

private:
  class Hash_table {
  public:
    // Robin hood hashing
    // No deletions
    // No rehashing
    // 64-bit keys, 32-bit values

    struct Entry {
      std::uint32_t key_hi{};
      std::uint32_t key_lo{};
      std::uint32_t value{};

      Entry() noexcept = default;

      explicit Entry(std::uint64_t key, std::uint32_t value) noexcept
          : key_hi{static_cast<std::uint32_t>(key >> 32)},
            key_lo{static_cast<std::uint32_t>(key)},
            value{value} {
        // if you're trying to construct the empty key, use the default ctor
        assert(key != 0);
      }

      constexpr bool empty() const noexcept {
        return key_hi == 0 && key_lo == 0;
      }

      constexpr std::uint64_t key() const noexcept {
        return (static_cast<std::uint64_t>(key_hi) << 32) | key_lo;
      }
    };

    explicit Hash_table(std::uint32_t bucket_count)
        : _buckets(std::make_unique<Entry[]>(bucket_count)),
          _bucket_count{bucket_count} {
      assert(bucket_count >= min_bucket_count);
      assert(bucket_count <= max_bucket_count);
      assert(std::has_single_bit(bucket_count));
    }

    Hash_table(Hash_table &&other) noexcept
        : _buckets{std::move(other._buckets)},
          _bucket_count{std::exchange(other._bucket_count, 0)} {}

    Hash_table &operator=(Hash_table &&other) noexcept {
      _buckets = std::move(other._buckets);
      _bucket_count = std::exchange(other._bucket_count, 0);
      return *this;
    }

    std::optional<std::uint32_t> get(std::uint64_t key) const noexcept {
      assert(key != 0 && "key must not be 0");
      if (_bucket_count == 0) {
        return std::nullopt;
      }
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      for (;;) {
        auto const &entry = _buckets[i];
        if (entry.empty()) {
          return std::nullopt;
        }
        if (entry.key() == key) {
          return entry.value;
        }
        i = (i + 1) & mask;
      }
    }

    void insert(std::uint64_t key, std::uint32_t value) noexcept {
      assert(key != 0 && "key must not be 0");
      auto entry = Entry{key, value};
      auto const mask = _bucket_count - 1;
      auto const bits = std::countr_one(mask);
      auto i = hash(key, bits);
      auto distance = std::uint32_t{};
      for (;;) {
        auto &bucket = _buckets[i];
        if (bucket.empty()) {
          bucket = entry;
          return;
        }
        assert(bucket.key() != entry.key() && "double inserts not allowed");
        auto const bucket_hash = hash(bucket.key(), bits);
        auto const bucket_distance = (i - bucket_hash) & mask;
        if (bucket_distance < distance) {
          std::swap(bucket, entry);
          distance = bucket_distance;
        }
        i = (i + 1) & mask;
        ++distance;
        assert(distance < _bucket_count && "must not insert into full table");
      }
    }

    static constexpr std::uint32_t min_bucket_count = 2;
    static constexpr std::uint32_t max_bucket_count = 0x80000000u;

  private:
    static constexpr std::uint64_t hash(std::uint64_t key, int bits) noexcept {
      auto const shift = 64 - bits;
      key ^= key >> shift;
      return (key * 11400714819323198485llu) >> shift;
    }

    std::unique_ptr<Entry[]> _buckets{};
    std::uint32_t _bucket_count{};
  };

  struct Indexed_keyframe {
    Keyframe keyframe;
    Hash_table camera_indices;
    Hash_table mesh_instance_indices;
  };

  struct Interpolation {
    void clear() {
      cameras.clear();
      mesh_instances.clear();
      valid = false;
    }

    std::vector<Identified<Camera>> cameras{};
    std::vector<Identified<Mesh_instance>> mesh_instances{};
    bool valid{};
  };

  static constexpr std::uint32_t
  object_count_to_bucket_count(std::uint32_t n) noexcept {
    return std::max(
      std::bit_ceil(static_cast<std::uint32_t>(std::ceil(n / 0.85))),
      Hash_table::min_bucket_count);
  }

  bool trim_old_keyframes() noexcept;

  std::size_t count_old_keyframes() const noexcept;

  bool interpolate();

  float _keyframe_duration;
  std::vector<Indexed_keyframe> _indexed_keyframes{};
  Interpolation _interpolation{};
  std::uint64_t _keyframe_number{};
  float _inter_keyframe_time{};
  bool _grid_remesh_flag{};
};

} // namespace fpsparty::scene

#endif
