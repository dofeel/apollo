/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 **/
#include "modules/planning/planning_base/reference_line/discrete_points_reference_line_smoother.h"

#include "gtest/gtest.h"
#include "modules/planning/planning_base/proto/reference_line_smoother_config.pb.h"
#include "modules/common/math/vec2d.h"
#include "modules/common/util/util.h"
#include "modules/map/hdmap/hdmap.h"
#include "modules/map/hdmap/hdmap_util.h"
#include "modules/planning/planning_base/reference_line/reference_line.h"
#include "modules/planning/planning_base/reference_line/reference_point.h"

namespace apollo {
namespace planning {

class DiscretePointsReferenceLineSmootherTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    smoother_.reset(new DiscretePointsReferenceLineSmoother(config_));
    hdmap_.LoadMapFromFile(map_file);
    const std::string lane_id = "1_-1";
    lane_info_ptr = hdmap_.GetLaneById(hdmap::MakeMapId(lane_id));
    if (!lane_info_ptr) {
      AERROR << "failed to find lane " << lane_id << " from map " << map_file;
      return;
    }
    std::vector<ReferencePoint> ref_points;
    const auto& points = lane_info_ptr->points();
    const auto& headings = lane_info_ptr->headings();
    const auto& accumulate_s = lane_info_ptr->accumulate_s();
    for (size_t i = 0; i < points.size(); ++i) {
      std::vector<hdmap::LaneWaypoint> waypoint;
      waypoint.emplace_back(lane_info_ptr, accumulate_s[i]);
      hdmap::MapPathPoint map_path_point(points[i], headings[i], waypoint);
      ref_points.emplace_back(map_path_point, 0.0, 0.0);
    }
    reference_line_.reset(new ReferenceLine(ref_points));
    vehicle_position_ = points[0];
  }

  const std::string map_file =
      "modules/planning/planning_base/testdata/garage_map/base_map.txt";

  hdmap::HDMap hdmap_;
  common::math::Vec2d vehicle_position_;
  ReferenceLineSmootherConfig config_;
  std::unique_ptr<ReferenceLineSmoother> smoother_;
  std::unique_ptr<ReferenceLine> reference_line_;
  hdmap::LaneInfoConstPtr lane_info_ptr = nullptr;
};

TEST_F(DiscretePointsReferenceLineSmootherTest, smooth) {
  ReferenceLine smoothed_reference_line;
  EXPECT_DOUBLE_EQ(153.87421515583992, reference_line_->Length());
  std::vector<AnchorPoint> anchor_points;
  const double interval = 10.0;
  int num_of_anchors =
      std::max(2, static_cast<int>(reference_line_->Length() / interval + 0.5));
  std::vector<double> anchor_s;
  common::util::uniform_slice(0.0, reference_line_->Length(),
                              num_of_anchors - 1, &anchor_s);
  for (const double s : anchor_s) {
    anchor_points.emplace_back();
    auto& last_anchor = anchor_points.back();
    auto ref_point = reference_line_->GetReferencePoint(s);
    last_anchor.path_point = ref_point.ToPathPoint(s);
    last_anchor.lateral_bound = 0.25;
    last_anchor.longitudinal_bound = 2.0;
  }
  anchor_points.front().longitudinal_bound = 1e-6;
  anchor_points.front().lateral_bound = 1e-6;
  anchor_points.back().longitudinal_bound = 1e-6;
  anchor_points.back().lateral_bound = 1e-6;
  smoother_->SetAnchorPoints(anchor_points);
  EXPECT_TRUE(smoother_->Smooth(*reference_line_, &smoothed_reference_line));
  EXPECT_NEAR(153.0, smoothed_reference_line.Length(), 1.0);
}

}  // namespace planning
}  // namespace apollo
