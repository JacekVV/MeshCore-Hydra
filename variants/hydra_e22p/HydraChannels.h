#pragma once

#include <Mesh.h>
#include <Print.h>

// Hydra: read-only view of public group channels (Public and hashtag channels from HYDRA_CHANNEL_TAGS).
// The repeater decrypts their messages and keeps the last HYDRA_MAX_MESSAGES in RAM, served as JSON at /channels.
#ifndef HYDRA_MAX_MESSAGES
  #define HYDRA_MAX_MESSAGES 50
#endif

void hydraChannelsInit();
int hydraSearchChannels(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches);
void hydraGroupMsg(const mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel, const uint8_t* data, size_t len);
void hydraChannelsJson(Print& out);
