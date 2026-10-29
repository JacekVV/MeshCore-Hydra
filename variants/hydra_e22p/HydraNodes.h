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
