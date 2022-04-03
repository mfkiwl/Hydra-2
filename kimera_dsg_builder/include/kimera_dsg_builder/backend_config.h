#pragma once
#include "kimera_dsg_builder/config_utils.h"
#include "kimera_dsg_builder/incremental_room_finder.h"

#include <KimeraRPGO/SolverParams.h>
#include <voxblox_ros/mesh_vis.h>

namespace kimera {
namespace incremental {

using RoomClusterModeEnum = RoomFinder::Config::ClusterMode;

}  // namespace incremental
}  // namespace kimera

DECLARE_CONFIG_ENUM(kimera::incremental,
                    RoomClusterModeEnum,
                    {RoomClusterModeEnum::SPECTRAL, "SPECTRAL"},
                    {RoomClusterModeEnum::MODULARITY, "MODULARITY"},
                    {RoomClusterModeEnum::NONE, "NONE"})

DECLARE_CONFIG_ENUM(KimeraRPGO,
                    Verbosity,
                    {Verbosity::UPDATE, "UPDATE"},
                    {Verbosity::QUIET, "QUIET"},
                    {Verbosity::VERBOSE, "VERBOSE"})

DECLARE_CONFIG_ENUM(KimeraRPGO, Solver, {Solver::LM, "LM"}, {Solver::GN, "GN"})

namespace kimera {
namespace incremental {

struct DsgBackendConfig {
  bool should_log = true;
  std::string log_path;

  bool visualize_place_factors = true;
  SemanticNodeAttributes::ColorVector building_color{169, 8, 194};  // purple
  SemanticNodeAttributes::Label building_semantic_label = 22u;

  bool enable_rooms = true;
  RoomFinder::Config room_finder;

  struct PgmoConfig {
    bool should_log = true;
    std::string log_path;
    // covariance
    double place_mesh_variance;
    double place_edge_variance;
    // rpgo
    bool gnc_fix_prev_inliers = true;
    KimeraRPGO::Verbosity rpgo_verbosity = KimeraRPGO::Verbosity::UPDATE;
    KimeraRPGO::Solver rpgo_solver = KimeraRPGO::Solver::LM;
  } pgmo;

  // dsg
  bool add_places_to_deformation_graph = true;
  bool optimize_on_lc = true;
  bool enable_node_merging = true;
  bool call_update_periodically = true;
  std::map<LayerId, bool> merge_update_map{{KimeraDsgLayers::OBJECTS, false},
                                           {KimeraDsgLayers::PLACES, true},
                                           {KimeraDsgLayers::ROOMS, false},
                                           {KimeraDsgLayers::BUILDINGS, false}};
  bool merge_update_dynamic = true;
  double places_merge_pos_threshold_m = 0.4;
  double places_merge_distance_tolerance_m = 0.3;
};

struct EnableMapConverter {
  using TargetMap = std::map<std::string, bool>;
  using SourceMap = std::map<LayerId, bool>;

  EnableMapConverter() = default;

  inline TargetMap from(const SourceMap& other) const {
    TargetMap to_return;
    for (const auto& kv_pair : other) {
      to_return[KimeraDsgLayers::LayerIdToString(kv_pair.first)] = kv_pair.second;
    }

    return to_return;
  }

  inline SourceMap to(const TargetMap& other) const {
    SourceMap to_return;
    for (const auto& kv_pair : other) {
      to_return[KimeraDsgLayers::StringToLayerId(kv_pair.first)] = kv_pair.second;
    }

    return to_return;
  }
};

template <typename Visitor>
void visit_config(const Visitor& v, DsgBackendConfig& config) {
  // TODO(nathan) replace with single param (derive should_log from log_path)
  v.visit("should_log", config.should_log);
  v.visit("log_path", config.log_path);
  v.visit("visualize_place_factors", config.visualize_place_factors);
  v.visit("building_color", config.building_color);
  v.visit("building_semantic_label", config.building_semantic_label);
  v.visit("enable_rooms", config.enable_rooms);
  v.visit("room_finder", config.room_finder);

  v.visit("pgmo", config.pgmo);

  auto dsg_handle = v["dsg"];
  dsg_handle.visit("add_places_to_deformation_graph",
                   config.add_places_to_deformation_graph);
  dsg_handle.visit("optimize_on_lc", config.optimize_on_lc);
  dsg_handle.visit("enable_node_merging", config.enable_node_merging);
  dsg_handle.visit("call_update_periodically", config.call_update_periodically);
  dsg_handle.visit("merge_update_map", config.merge_update_map, EnableMapConverter());
  dsg_handle.visit("merge_update_dynamic", config.merge_update_dynamic);
  dsg_handle.visit("places_merge_pos_threshold_m", config.places_merge_pos_threshold_m);
  dsg_handle.visit("places_merge_distance_tolerance_m",
                   config.places_merge_distance_tolerance_m);
}

template <typename Visitor>
void visit_config(const Visitor& v, DsgBackendConfig::PgmoConfig& config) {
  // TODO(nathan) replace with single param (derive should_log from log_path)
  v.visit("should_log", config.should_log);
  v.visit("log_path", config.log_path);
  auto covar_handle = v["covariance"];
  covar_handle.visit("place_mesh", config.place_mesh_variance);
  covar_handle.visit("place_edge", config.place_edge_variance);
  auto rpgo_handle = v["rpgo"];
  rpgo_handle.visit("gnc_fix_prev_inliers", config.gnc_fix_prev_inliers);
  rpgo_handle.visit("verbosity", config.rpgo_verbosity);
  rpgo_handle.visit("solver", config.rpgo_solver);
}

template <typename Visitor>
void visit_config(const Visitor& v, RoomFinder::Config& config) {
  v.visit("min_dilation_m", config.min_dilation_m);
  v.visit("max_dilation_m", config.max_dilation_m);
  v.visit("num_steps", config.num_steps);
  v.visit("min_component_size", config.min_component_size);
  v.visit("room_semantic_label", config.room_semantic_label);
  v.visit("max_kmeans_iters", config.max_kmeans_iters);
  v.visit("room_vote_min_overlap", config.room_vote_min_overlap);
  v.visit("min_room_size", config.min_room_size);
  v.visit("max_modularity_iters", config.max_modularity_iters);
  v.visit("modularity_gamma", config.modularity_gamma);
  v.visit("use_previous_rooms", config.use_previous_rooms);
  v.visit("clustering_mode", config.clustering_mode);

  std::string prefix_string;
  v.visit("room_prefix", prefix_string);
  if (config_parser::is_parser<Visitor>()) {
    config.room_prefix = prefix_string[0];
  }
}

}  // namespace incremental
}  // namespace kimera

DECLARE_CONFIG_OSTREAM_OPERATOR(kimera::incremental, RoomFinder::Config)
DECLARE_CONFIG_OSTREAM_OPERATOR(kimera::incremental, DsgBackendConfig)
