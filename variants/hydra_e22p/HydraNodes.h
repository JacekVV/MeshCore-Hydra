#pragma once

#include <Mesh.h>
#include <Print.h>

// Hydra: list of nodes heard by this repeater, built from received adverts (like the Meshtastic node list).
// Filled from MyMesh::onAdvertRecv(), served as JSON at http://<ip>/nodes by HydraBoard.
#ifndef HYDRA_MAX_NODES
  #define HYDRA_MAX_NODES 100
#endif

void hydraNodeHeard(const mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len);
void hydraNodesJson(Print& out);

// Received packets with route type, path and payload (duplicates too, each copy may come another way), the last
// HYDRA_MAX_PATHS, served as JSON at /paths. The RPi draws the "who hears whom" map from the flood paths and lists
// all packets with what can be read without keys. Hook in MyMesh::onRecvPacket().
#ifndef HYDRA_MAX_PATHS
  #define HYDRA_MAX_PATHS 80
#endif

void hydraPathHeard(const mesh::Packet* packet, const uint8_t* self_pub_key, double self_lat, double self_lon);
void hydraPathsJson(Print& out);
