#pragma once
#include "kimera_scene_graph/common.h"

#include <kimera_dsg/scene_graph.h>
#include <kimera_semantics/common.h>
#include <ros/ros.h>
#include <voxblox/core/layer.h>
#include <voxblox/core/voxel.h>
#include <voxblox/mesh/mesh.h>

namespace kimera {

struct RoomPclClusters {
  using CloudCentroidPair = std::pair<ColorPointCloud::Ptr, Centroid>;
  IntensityPointCloud::Ptr colored_room_cloud;
  std::vector<CloudCentroidPair> room_info;
};

class RoomFinder {
 public:
  /**
   * @brief RoomFinder
   * @param nh_private
   * @param world_frame
   * @param esdf_slice_level Height at which the ESDF slice is computed for room
   * clustering.
   */
  RoomFinder(const ros::NodeHandle& nh_private,
             const std::string& world_frame,
             vxb::FloatingPoint esdf_slice_level,
             bool visualize = false);

  ~RoomFinder() = default;

  /**
   * @brief findRooms Uses Semantic ESDF
   * @param esdf_layer
   * @param[out] Scene Graph to be updated with Room layer.
   * @return
   */
  IntensityPointCloud::Ptr findRooms(
      const vxb::Layer<vxb::EsdfVoxel>& esdf_layer,
      SceneGraph* scene_graph);

 private:
  /**
   * @brief updateSceneGraph Updates the scene graph with the room instances.
   * It does not estimate inter-room connectivity, nor parent/children
   * connectivity
   * @param[in] room_centroids point clouds and centroids of each room
   * @param[out] scene_graph Scene graph to be updated
   */
  void updateSceneGraph(const RoomPclClusters& rooms,
                        SceneGraph* scene_graph);

  /**
   * @brief publishTruncatedEsdf For visualization only. Publishes the truncated
   * ESDF slice to visualize the layout of the room (previous to segmentation).
   * @param esdf_pcl Pointcloud extracted from the ESDF layer
   */
  void publishTruncatedEsdf(const IntensityPointCloud::Ptr& esdf_pcl);

 private:
  ros::NodeHandle nh_private_;
  ros::Publisher pcl_pub_;
  ros::Publisher esdf_truncated_pub_;

  std::string world_frame_;

  // Height where to cut the ESDF for room segmentation
  vxb::FloatingPoint esdf_slice_level_;

  // Counter
  NodeSymbol next_room_id_;

  bool visualize_;
};

}  // namespace kimera