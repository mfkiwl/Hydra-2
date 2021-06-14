#include "kimera_scene_graph/object_finder.h"
#include "kimera_scene_graph/common.h"

#include <actionlib/client/terminal_state.h>
#include <kimera_dsg/bounding_box.h>
#include <kimera_semantics/common.h>

#include <pcl/ModelCoefficients.h>
#include <pcl/common/centroid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/search/kdtree.h>
//#include <pcl/search/search.h>

#include <ostream>

namespace kimera {

std::ostream& operator<<(std::ostream& out,
                         const RegionGrowingClusteringParams& p) {
  // clang-format off
  out << "\n================== Region Growing Cluster Config ====================\n";
  out << " - normal_estimator_neighbour_size:  " << p.normal_estimator_neighbour_size << '\n';
  out << " - min_cluster_size:                 " << p.min_cluster_size << '\n';
  out << " - max_cluster_size:                 " << p.max_cluster_size << '\n';
  out << " - number_of_neighbours:             " << p.number_of_neighbours << '\n';
  out << " - smoothness_threshold_:            " << p.smoothness_threshold << '\n';
  out << " - curvature_threshold_:             " << p.curvature_threshold << '\n';
  out << "==============================================================\n";
  // clang-format on
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const EuclideanClusteringParams& p) {
  std::stringstream ss;
  // clang-format off
  out << "\n================== Euclidean Cluster Config ====================\n";
  out << " - min_cluster_size:                 " << p.min_cluster_size << '\n';
  out << " - max_cluster_size:                 " << p.max_cluster_size << '\n';
  out << " - cluster_tolerance:                " << p.cluster_tolerance << '\n';
  out << "==============================================================\n";
  // clang-format on
  return out;
}

ObjectFinder::ObjectFinder(const std::string& world_frame,
                           ObjectFinderType type)
    : world_frame_(world_frame), type_(type), next_object_id_('O', 0) {
  setupRegionGrowingClusterEstimator();
  setupEuclideanClusterEstimator();
}

void ObjectFinder::connectToObjectDb() {
  // Build Object database action client
  ROS_INFO("Creating object database client");
  object_db_client_ = kimera::make_unique<ObjectDBClient>("/object_db", true);
  ROS_INFO("Waiting for object database server");
  object_db_client_->waitForServer();
  ROS_INFO("Object database server connected");
}

void ObjectFinder::addObjectsToGraph(const ColorPointCloud::Ptr& input,
                                     const NodeColor& label_color,
                                     SemanticLabel label,
                                     SceneGraph* scene_graph) {
  CHECK(scene_graph);
  CHECK(input);

  VLOG(1) << "Extracting objects for label: " << std::to_string(label);
  if (input->empty()) {
    VLOG(1) << "Skipping label " << label << " as input was empty";
    return;
  }

  // TODO(nathan) eventually we'll refactor this to group centroid and pcls
  Centroids centroids;
  ObjectPointClouds objects;
  BoundingBoxes bounding_boxes;
  findObjects(input, &centroids, &objects, &bounding_boxes);
  // TODO(nathan) used to publish the clustered pointcloud?
  // color_clustered_pcl_pub_.publish();

  // TODO(nathan) use the registration for something
  // ObjectPointClouds registered_objects =
  // registerObjects(objects, std::to_string(label));

  // Create semantic instance for each centroid
  for (size_t idx = 0u; idx < centroids.size(); ++idx) {
    ObjectNodeAttributes::Ptr attrs = std::make_unique<ObjectNodeAttributes>();
    attrs->semantic_label = label;
    attrs->color = label_color,
    attrs->name = std::to_string(label) + std::to_string(idx);
    attrs->points = objects.at(idx);
    attrs->bounding_box = bounding_boxes.at(idx);

    pcl::PointXYZ centroid;
    centroids.at(idx).get(centroid);
    attrs->position << centroid.x, centroid.y, centroid.z;

    scene_graph->emplaceNode(to_underlying(KimeraDsgLayers::OBJECTS),
                             next_object_id_,
                             std::move(attrs));
    ++next_object_id_;
  }
}

ColorPointCloud::Ptr ObjectFinder::findObjects(
    const ColorPointCloud::Ptr& pointcloud,
    Centroids* centroids,
    ObjectPointClouds* object_pcls,
    BoundingBoxes* bounding_boxes) {
  CHECK(pointcloud);
  CHECK_NOTNULL(centroids);
  CHECK_NOTNULL(object_pcls);
  CHECK_NOTNULL(bounding_boxes);
  ColorPointCloud::Ptr clustered_colored_pcl = nullptr;
  switch (type_) {
    case ObjectFinderType::kRegionGrowing:
      VLOG(2) << "Using region growing object finder.";
      clustered_colored_pcl =
          regionGrowingClusterEstimator(pointcloud, centroids, object_pcls);
      break;
    case ObjectFinderType::kEuclidean:
    default:
      VLOG(2) << "Using euclidean object finder.";
      clustered_colored_pcl =
          euclideanClusterEstimator(pointcloud, centroids, object_pcls);
      break;
  }
  CHECK(clustered_colored_pcl);

  // Find BB
  bounding_boxes->resize(object_pcls->size());
  for (size_t i = 0; i < object_pcls->size(); ++i) {
    bounding_boxes->at(i) =
        BoundingBox::extract(object_pcls->at(i), BoundingBox::Type::AABB);
  }

  clustered_colored_pcl->header.frame_id = world_frame_;
  return clustered_colored_pcl;
}

void ObjectFinder::updateClusterEstimator(ObjectFinderType type) {
  type_ = type;
}

void ObjectFinder::updateRegionGrowingParams(
    const RegionGrowingClusteringParams& new_params) {
  region_growing_params_ = new_params;
  setupRegionGrowingClusterEstimator();
}

void ObjectFinder::updateEuclideanClusterParams(
    const EuclideanClusteringParams& new_params) {
  euclidean_params_ = new_params;
  setupEuclideanClusterEstimator();
}

std::ostream& operator<<(std::ostream& out, const ObjectFinder& finder) {
  switch (static_cast<ObjectFinderType>(finder.type_)) {
    case ObjectFinderType::kEuclidean:
      out << "Object Finder: Euclidean";
      out << finder.euclidean_params_;
      break;
    case ObjectFinderType::kRegionGrowing:
      out << "Object Finder: Region growing";
      out << finder.region_growing_params_;
      break;
    default:
      out << "Object Finder: Unkown object finder type";
      break;
  }
  return out;
}

void ObjectFinder::setupRegionGrowingClusterEstimator() {
  region_growing_estimator_.setMinClusterSize(
      region_growing_params_.min_cluster_size);
  region_growing_estimator_.setMaxClusterSize(
      region_growing_params_.max_cluster_size);
  region_growing_estimator_.setNumberOfNeighbours(
      region_growing_params_.number_of_neighbours);
  region_growing_estimator_.setSmoothnessThreshold(
      region_growing_params_.smoothness_threshold);
  region_growing_estimator_.setCurvatureThreshold(
      region_growing_params_.curvature_threshold);
  CHECK(region_growing_estimator_.getCurvatureTestFlag());
  CHECK(region_growing_estimator_.getSmoothModeFlag());
}

void ObjectFinder::setupEuclideanClusterEstimator() {
  euclidean_estimator_.setClusterTolerance(euclidean_params_.cluster_tolerance);
  euclidean_estimator_.setMinClusterSize(euclidean_params_.min_cluster_size);
  euclidean_estimator_.setMaxClusterSize(euclidean_params_.max_cluster_size);
}

ColorPointCloud::Ptr ObjectFinder::regionGrowingClusterEstimator(
    const ColorPointCloud::Ptr& cloud,
    Centroids* centroids,
    ObjectPointClouds* object_pcls) {
  CHECK(cloud);
  CHECK_NOTNULL(centroids);
  CHECK_NOTNULL(object_pcls);
  pcl::search::Search<ColorPoint>::Ptr tree(
      new pcl::search::KdTree<ColorPoint>);
  pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
  pcl::NormalEstimation<ColorPoint, pcl::Normal> normal_estimator;
  normal_estimator.setSearchMethod(tree);
  normal_estimator.setKSearch(
      region_growing_params_.normal_estimator_neighbour_size);

  normal_estimator.setInputCloud(cloud);
  normal_estimator.compute(*normals);

  region_growing_estimator_.setSearchMethod(tree);

  region_growing_estimator_.setInputCloud(cloud);
  region_growing_estimator_.setInputNormals(normals);

  std::vector<pcl::PointIndices> cluster_indices;
  region_growing_estimator_.extract(cluster_indices);

  // Get centroids of clusters.
  getCentroidsGivenClusters(cloud, cluster_indices, centroids, object_pcls);

  LOG(INFO) << "Number of clusters found: " << cluster_indices.size();
  return region_growing_estimator_.getColoredCloud();
}

ColorPointCloud::Ptr ObjectFinder::euclideanClusterEstimator(
    const ColorPointCloud::Ptr& cloud,
    Centroids* centroids,
    ObjectPointClouds* object_pcls) {
  CHECK_NOTNULL(centroids);
  CHECK_NOTNULL(object_pcls);
  pcl::search::KdTree<ColorPoint>::Ptr tree(
      new pcl::search::KdTree<ColorPoint>);
  tree->setInputCloud(cloud);

  euclidean_estimator_.setSearchMethod(tree);
  euclidean_estimator_.setInputCloud(cloud);

  // Extract clusters
  std::vector<pcl::PointIndices> cluster_indices;
  euclidean_estimator_.extract(cluster_indices);

  // Get centroids of clusters.
  getCentroidsGivenClusters(cloud, cluster_indices, centroids, object_pcls);

  return getColoredCloud(cloud, cluster_indices);
}

ColorPointCloud::Ptr ObjectFinder::getColoredCloud(
    const ColorPointCloud::Ptr& input,
    const std::vector<pcl::PointIndices>& clusters) {
  ColorPointCloud::Ptr colored_cloud(new ColorPointCloud);

  if (!clusters.empty()) {
    srand(static_cast<unsigned int>(time(nullptr)));
    std::vector<unsigned char> colors;
    for (std::size_t i_segment = 0; i_segment < clusters.size(); i_segment++) {
      colors.push_back(static_cast<unsigned char>(rand() % 256));
      colors.push_back(static_cast<unsigned char>(rand() % 256));
      colors.push_back(static_cast<unsigned char>(rand() % 256));
    }

    colored_cloud->width = input->width;
    colored_cloud->height = input->height;
    colored_cloud->is_dense = input->is_dense;
    for (std::size_t i_point = 0; i_point < input->points.size(); i_point++) {
      ColorPoint point;
      point.x = *(input->points[i_point].data);
      point.y = *(input->points[i_point].data + 1);
      point.z = *(input->points[i_point].data + 2);
      point.r = 255;
      point.g = 0;
      point.b = 0;
      colored_cloud->points.push_back(point);
    }

    int next_color = 0;
    for (auto i_segment = clusters.cbegin(); i_segment != clusters.cend();
         i_segment++) {
      for (auto i_point = i_segment->indices.cbegin();
           i_point != i_segment->indices.cend();
           i_point++) {
        int index;
        index = *i_point;
        colored_cloud->points[index].r = colors[3 * next_color];
        colored_cloud->points[index].g = colors[3 * next_color + 1];
        colored_cloud->points[index].b = colors[3 * next_color + 2];
      }
      next_color++;
    }
  }

  return colored_cloud;
}

void ObjectFinder::getCentroidsGivenClusters(
    const ColorPointCloud::Ptr& cloud,
    const std::vector<pcl::PointIndices>& cluster_indices,
    Centroids* centroids,
    ObjectPointClouds* object_pcls) {
  CHECK_NOTNULL(centroids);
  CHECK_NOTNULL(object_pcls);
  centroids->resize(cluster_indices.size());
  object_pcls->resize(cluster_indices.size());
  for (size_t k = 0; k < cluster_indices.size(); ++k) {
    Centroid& centroid = centroids->at(k);
    ColorPointCloud::Ptr pcl(new ColorPointCloud());
    const auto& indices = cluster_indices.at(k).indices;
    pcl->resize(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
      // For centroid of cluster k, add all points belonging to it.
      const ColorPoint& color_point = cloud->at(indices.at(i));
      centroid.add(Point(color_point.x, color_point.y, color_point.z));
      pcl->at(i) = color_point;
    }
    object_pcls->at(k) = pcl;
  }
}

ObjectPointClouds ObjectFinder::registerObjects(
    const ObjectPointClouds& object_pcls,
    const std::string semantic_label) {
  CHECK(object_db_client_);
  LOG(INFO) << "Sending object point clouds to object database.";
  // For storing all registrated object point clouds
  ObjectPointClouds registrated_object_pcls;

  // Query object database for all point clouds
  // Object pcls is a vector of pointers to pcl::XYZRGB point clouds
  for (const auto& color_pcl : object_pcls) {
    if (color_pcl->points.size() == 0) {
      LOG(INFO) << "Empty point cloud given to object database client.";
      registrated_object_pcls.push_back(color_pcl);
      continue;
    }
    LOG(INFO) << "Object pcl size:" << color_pcl->points.size();

    // Create goal for object db
    object_db::ObjectRegistrationGoal c_goal;
    c_goal.semantic_label = semantic_label;

    // Get current color
    const auto& color_r = color_pcl->points[0].r;
    const auto& color_g = color_pcl->points[0].g;
    const auto& color_b = color_pcl->points[0].b;

    // Convert colored point cloud to sensor msg point cloud
    for (size_t p_idx = 0; p_idx < color_pcl->size(); ++p_idx) {
      geometry_msgs::Point32 c_point;
      c_point.x = color_pcl->points[p_idx].x;
      c_point.y = color_pcl->points[p_idx].y;
      c_point.z = color_pcl->points[p_idx].z;
      c_goal.dst.points.push_back(c_point);
    }

    // Send goal to object databse action server
    object_db_client_->sendGoal(c_goal);

    // Deal with the result
    bool finished = object_db_client_->waitForResult(ros::Duration(30));
    bool aborted = object_db_client_->getState() ==
                   actionlib::SimpleClientGoalState::ABORTED;
    if (aborted) {
      LOG(INFO) << "Object database aborted.";
      registrated_object_pcls.push_back(color_pcl);
    } else if (!finished) {
      LOG(INFO) << "Object database did not finish before the timeout.";
      registrated_object_pcls.push_back(color_pcl);
    } else {
      auto result = object_db_client_->getResult();
      auto registrated_object = result->aligned_object;

      // Convert sensor msg point cloud type to colored pcl
      ColorPointCloud::Ptr registrated_pcl(new ColorPointCloud);
      for (const auto& o_point : registrated_object.points) {
        ColorPoint c_point;
        c_point.x = o_point.x;
        c_point.y = o_point.y;
        c_point.z = o_point.z;
        c_point.r = color_r;
        c_point.g = color_g;
        c_point.b = color_b;
        registrated_pcl->points.push_back(c_point);
      }
      registrated_object_pcls.push_back(registrated_pcl);
      LOG(INFO) << "Object database query successful.";
      LOG(INFO) << "Registrated object size: " << registrated_pcl->size();
    }
  }

  if (registrated_object_pcls.size() != object_pcls.size()) {
    LOG(INFO) << "Registrated objects size mismatch!";
  }
  return registrated_object_pcls;
}

}  // namespace kimera