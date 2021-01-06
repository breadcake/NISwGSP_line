//
//  Transform.cpp
//  UglyMan_Stitching
//
//  Created by uglyman.nothinglo on 2015/8/15.
//  Copyright (c) 2015 nothinglo. All rights reserved.
//

#define _USE_MATH_DEFINES // for C++
#include <cmath>

#include "Transform.h"

cv::Mat getConditionerFromPts(const vector<Point2> & pts) {
    cv::Mat pts_ref(pts);
    cv::Scalar mean_pts, std_pts;
    meanStdDev(pts_ref, mean_pts, std_pts);
    
    std_pts = (std_pts.mul(std_pts) * pts_ref.rows / (double)(pts_ref.rows - 1));
    sqrt(std_pts, std_pts);
    
    std_pts.val[0] = std_pts.val[0] + (std_pts.val[0] == 0);
    std_pts.val[1] = std_pts.val[1] + (std_pts.val[1] == 0);
    
	cv::Mat result(3, 3, CV_64FC1);
    result.at<double>(0, 0) = sqrt(2)   / (double)std_pts.val[0];
    result.at<double>(0, 1) = 0;
    result.at<double>(0, 2) = -(sqrt(2) / (double)std_pts.val[0]) * (double)mean_pts.val[0];
    
    result.at<double>(1, 0) = 0;
    result.at<double>(1, 1) = sqrt(2)   / (double)std_pts.val[1];
    result.at<double>(1, 2) = -(sqrt(2) / (double)std_pts.val[1]) * (double)mean_pts.val[1];
    
    result.at<double>(2, 0) = 0;
    result.at<double>(2, 1) = 0;
    result.at<double>(2, 2) = 1;
    
    return result;
}
cv::Mat getNormalize2DPts(const vector<Point2> & pts, vector<Point2> & newpts) {
    cv::Mat pts_ref(pts), npts;
    cv::Scalar mean_p = mean(pts_ref);
    npts = pts_ref - mean_p;
	cv::Mat dist = npts.mul(npts);
    dist = dist.reshape(1);
    sqrt(dist.col(0) + dist.col(1), dist);
    double scale = sqrt(2) / mean(dist).val[0];
    
	cv::Mat result(3, 3, CV_64FC1);
    result.at<double>(0, 0) = scale;
    result.at<double>(0, 1) = 0;
    result.at<double>(0, 2) = -scale * (double)mean_p.val[0];
    
    result.at<double>(1, 0) = 0;
    result.at<double>(1, 1) = scale;
    result.at<double>(1, 2) = -scale * (double)mean_p.val[1];
    
    result.at<double>(2, 0) = 0;
    result.at<double>(2, 1) = 0;
    result.at<double>(2, 2) = 1;
    
#ifndef NDEBUG
    if(newpts.empty() == false) {
        newpts.clear();
        printError("F(getNormalize2DPts) newpts is not empty");
    }
#endif
    newpts.reserve(pts.size());
    for(int i = 0; i < pts.size(); ++i) {
        newpts.emplace_back(pts[i].x * result.at<double>(0, 0) + result.at<double>(0, 2),
                            pts[i].y * result.at<double>(1, 1) + result.at<double>(1, 2));
    }
    
    return result;
}

template <typename T>
T normalizeAngle(T x) {
    x = fmod(x + 180, 360);
    if (x < 0) {
        x += 360;
    }
    return x - 180;
}

template <typename T>
cv::Point_<T> applyTransform3x3(T x, T y, const cv::Mat & matT) {
    double denom = 1. / (matT.at<double>(2, 0) * x + matT.at<double>(2, 1) * y + matT.at<double>(2, 2));
    return cv::Point_<T>((matT.at<double>(0, 0) * x + matT.at<double>(0, 1) * y + matT.at<double>(0, 2)) * denom,
                     (matT.at<double>(1, 0) * x + matT.at<double>(1, 1) * y + matT.at<double>(1, 2)) * denom);
}

template <typename T>
cv::Point_<T> applyTransform2x3(T x, T y, const cv::Mat & matT) {
    return cv::Point_<T>((matT.at<double>(0, 0) * x + matT.at<double>(0, 1) * y + matT.at<double>(0, 2)),
                     (matT.at<double>(1, 0) * x + matT.at<double>(1, 1) * y + matT.at<double>(1, 2)));
}

template <typename T>
cv::Size_<T> normalizeVertices(vector<vector<cv::Point_<T> > > & vertices) {
    T min_x = std::numeric_limits<T>::max(), max_x = -std::numeric_limits<T>::max();
    T min_y = std::numeric_limits<T>::max(), max_y = -std::numeric_limits<T>::max();
    for(int i = 0; i < vertices.size(); ++i) {
        for(int j = 0; j < vertices[i].size(); ++j) {
            min_x = min(min_x, vertices[i][j].x);
            min_y = min(min_y, vertices[i][j].y);
            max_x = max(max_x, vertices[i][j].x);
            max_y = max(max_y, vertices[i][j].y);
        }
    }
    for(int i = 0; i < vertices.size(); ++i) {
        for(int j = 0; j < vertices[i].size(); ++j) {
            vertices[i][j].x = (vertices[i][j].x - min_x);
            vertices[i][j].y = (vertices[i][j].y - min_y);
        }
    }
    return cv::Size_<T>(max_x - min_x, max_y - min_y);
}

template <typename T>
cv::Rect_<T> getVerticesRects(const vector<cv::Point_<T> > & vertices) {
    vector<vector<cv::Point_<T> > > tmp(1, vertices);
    return getVerticesRects(tmp).front();
}

template <typename T>
vector<cv::Rect_<T> > getVerticesRects(const vector<vector<cv::Point_<T> > > & vertices) {
    vector<cv::Rect_<T> > result;
    result.reserve(vertices.size());
    for(int i = 0; i < vertices.size(); ++i) {
        T min_ix = MAXFLOAT, max_ix = -MAXFLOAT;
        T min_iy = MAXFLOAT, max_iy = -MAXFLOAT;
        for(int j = 0; j < vertices[i].size(); ++j) {
            min_ix = min(min_ix, vertices[i][j].x);
            max_ix = max(max_ix, vertices[i][j].x);
            min_iy = min(min_iy, vertices[i][j].y);
            max_iy = max(max_iy, vertices[i][j].y);
        }
        result.emplace_back(min_ix, min_iy,
                            max_ix - min_ix, max_iy - min_iy);
    }
    return result;
}

template <typename T>
T getSubpix(const cv::Mat & img, const cv::Point2f & pt) {
	cv::Mat patch;
    cv::getRectSubPix(img, cv::Size(1,1), pt, patch);
    return patch.at<T>(0,0);
}

template <typename T, size_t n>
cv::Vec<T, n> getSubpix(const cv::Mat & img, const cv::Point2f & pt) {
	cv::Mat patch;
    cv::getRectSubPix(img, cv::Size(1,1), pt, patch);
    return patch.at<cv::Vec<T, n> >(0,0);
}

template <typename T>
cv::Vec<T, 3> getEulerZXYRadians(const cv::Mat_<T> & rot_matrix) {
    const T r00 = rot_matrix.template at<T>(0, 0);
    const T r01 = rot_matrix.template at<T>(0, 1);
    const T r02 = rot_matrix.template at<T>(0, 2);
    const T r10 = rot_matrix.template at<T>(1, 0);
    const T r11 = rot_matrix.template at<T>(1, 1);
    const T r12 = rot_matrix.template at<T>(1, 2);
    const T r22 = rot_matrix.template at<T>(2, 2);
    
	cv::Vec<T, 3> result;
    if(r12 < 1) {
        if(r12 > -1) {
            result[0] = asin(-r12);
            result[1] = atan2(r02, r22);
            result[2] = atan2(r10, r11);
        } else {
            result[0] = M_PI_2;
            result[1] = -atan2(-r01, r00);
            result[2] = 0.;
        }
    } else {
        result[0] = -M_PI_2;
        result[1] = -atan2(-r01, r00);
        result[2] = 0.;
    }
    return result;
}

template <typename T>
bool isEdgeIntersection(const cv::Point_<T> & src_1, const cv::Point_<T> & dst_1,
                        const cv::Point_<T> & src_2, const cv::Point_<T> & dst_2,
                        double * scale_1, double * scale_2) {
    const cv::Point_<T> s1 = dst_1 - src_1, s2 = dst_2 - src_2;
    const double denom = -s2.x * s1.y + s1.x * s2.y;
    
    if(denom <  std::numeric_limits<double>::epsilon() &&
       denom > -std::numeric_limits<double>::epsilon()) {
        return false;
    }
    
    double tmp_scale_1 = ( s2.x * (src_1.y - src_2.y) - s2.y * (src_1.x - src_2.x)) / denom;
    double tmp_scale_2 = (-s1.y * (src_1.x - src_2.x) + s1.x * (src_1.y - src_2.y)) / denom;
    
    if(scale_1) *scale_1 = tmp_scale_1;
    if(scale_2) *scale_2 = tmp_scale_2;
    
    return (tmp_scale_1 >= 0 && tmp_scale_1 <= 1 &&
            tmp_scale_2 >= 0 && tmp_scale_2 <= 1);
}

template <typename T>
bool isRotationInTheRange(const T rotation, const T min_rotation, const T max_rotation) {
    const cv::Point_<T> b(cos(rotation), sin(rotation));
    const cv::Point_<T> a(cos(min_rotation), sin(min_rotation));
    const cv::Point_<T> c(cos(max_rotation), sin(max_rotation));
    const T direction_a_b = a.x * b.y - a.y * b.x;
    const T direction_a_c = a.x * c.y - a.y * c.x;
    const T direction_b_c = b.x * c.y - b.y * c.x;
    
    return (direction_a_b * direction_a_c >= 0) && (direction_a_b * direction_b_c >= 0);
}

template  float normalizeAngle< float>( float x);
template double normalizeAngle<double>(double x);
    
template cv::Point_< float> applyTransform3x3< float>( float x,  float y, const cv::Mat & matT);
template cv::Point_<double> applyTransform3x3<double>(double x, double y, const cv::Mat & matT);

template cv::Point_< float> applyTransform2x3< float>( float x,  float y, const cv::Mat & matT);
template cv::Point_<double> applyTransform2x3<double>(double x, double y, const cv::Mat & matT);

template cv::Size_<   int> normalizeVertices<   int>(vector<vector<cv::Point_<   int> > > & vertices);
template cv::Size_< float> normalizeVertices< float>(vector<vector<cv::Point_< float> > > & vertices);
template cv::Size_<double> normalizeVertices<double>(vector<vector<cv::Point_<double> > > & vertices);

template cv::Rect_< float> getVerticesRects< float>(const vector<cv::Point_< float> > & vertices);
template cv::Rect_<double> getVerticesRects<double>(const vector<cv::Point_<double> > & vertices);

template vector<cv::Rect_< float> > getVerticesRects< float>(const vector<vector<cv::Point_< float> > > & vertices);
template vector<cv::Rect_<double> > getVerticesRects<double>(const vector<vector<cv::Point_<double> > > & vertices);

template          float getSubpix<   float>(const cv::Mat & img, const cv::Point2f & pt);
template cv::Vec< uchar, 1> getSubpix<uchar, 1>(const cv::Mat & img, const cv::Point2f & pt);
template cv::Vec< uchar, 3> getSubpix<uchar, 3>(const cv::Mat & img, const cv::Point2f & pt);

template cv::Vec< float, 3> getEulerZXYRadians< float>(const cv::Mat_< float> & rot_matrix);
template cv::Vec<double, 3> getEulerZXYRadians<double>(const cv::Mat_<double> & rot_matrix);

template bool isEdgeIntersection< float>(const cv::Point_< float> & src_1, const cv::Point_< float> & dst_1,
                                         const cv::Point_< float> & src_2, const cv::Point_< float> & dst_2,
                                         double * scale_1, double * scale_2);

template bool isEdgeIntersection<double>(const cv::Point_<double> & src_1, const cv::Point_<double> & dst_1,
                                         const cv::Point_<double> & src_2, const cv::Point_<double> & dst_2,
                                         double * scale_1, double * scale_2);

template bool isRotationInTheRange< float>(const  float rotation, const  float min_rotation, const  float max_rotation);
template bool isRotationInTheRange<double>(const double rotation, const double min_rotation, const double max_rotation);


