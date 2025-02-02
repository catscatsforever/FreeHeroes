/*
 * Copyright (C) 2023 Smirnov Vladimir / mapron1@gmail.com
 * SPDX-License-Identifier: MIT
 * See LICENSE file for details.
 */

#include "KMeans.hpp"

#include <omp.h>
#include <iostream>

namespace FreeHeroes {

KMeansSegmentation::Cluster* KMeansSegmentation::getNearestClusterId(Point& point)
{
    int64_t  minDist          = m_clusters[0].distanceTo(point);
    Cluster* nearestClusterId = &m_clusters[0];

    for (size_t i = 1; i < m_clusters.size(); i++) {
        const int64_t dist = m_clusters[i].distanceTo(point);
        if (dist < minDist) {
            minDist          = dist;
            nearestClusterId = &m_clusters[i];
        }
    }

    return nearestClusterId;
}

void KMeansSegmentation::runIter()
{
    bool done = true;

    // Add all points to their nearest cluster
    for (size_t i = 0, cnt = m_points.size(); i < cnt; i++) {
        Cluster* currentClusterId = m_points[i].getCluster();
        Cluster* nearestClusterId = getNearestClusterId(m_points[i]);

        if (currentClusterId != nearestClusterId) {
            m_points[i].setCluster(nearestClusterId);
            done = false;
        }
    }

    // clear all existing clusters
    clearClusters();

    // reassign points to their new clusters
    for (size_t i = 0; i < m_points.size(); i++) {
        m_points[i].getCluster()->addPoint(&m_points[i]);
    }

    // Recalculating the center of each cluster
    for (size_t i = 0, cnt = m_clusters.size(); i < cnt; i++) {
        const size_t clusterSize = m_clusters[i].getSize();
        const auto&  cluster     = m_clusters[i];
        if (cluster.m_speed == 0)
            continue;

        int64_t sumX = 0;
        int64_t sumY = 0;
        if (clusterSize > 0) {
            for (size_t p = 0; p < clusterSize; p++) {
                sumX += cluster.getPoint(p)->m_pos->m_pos.m_x;
                sumY += cluster.getPoint(p)->m_pos->m_pos.m_y;
            }
            m_clusters[i].m_centroid = { int(sumX / clusterSize), int(sumY / clusterSize) };
        }
    }

    m_done = done;
}

void KMeansSegmentation::run(std::ostream& os)
{
    //os << "Before K-Means Clustering: \n";

    //for (size_t i = 0; i < m_clusters.size(); i++)
    //    os << "Cluster " << m_clusters[i].m_index << " centroid: " << m_clusters[i].getCentroidStr() << std::endl;

    int iter;
    for (iter = 0; iter < m_iters; ++iter) {
        runIter();
        if (m_done)
            break;
    }

    //os << "K-Means Done on " << iter << " iteration\n";

    //for (size_t i = 0; i < m_clusters.size(); i++)
    //    os << "Cluster " << m_clusters[i].m_index << " centroid: " << m_clusters[i].getCentroidStr() << std::endl;

    for (int i = (int) m_clusters.size() - 1; i >= 0; i--) {
        if (m_clusters[i].m_points.size() == 0) {
            m_clusters.erase(m_clusters.begin() + i);
        }
    }
}

}
