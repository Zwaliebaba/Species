#pragma once

// The inbound half of the game-to-protocol boundary.
//
// ClientToServer is transport: an inbox, an outbox, sockets and sequence ids.
// Applying a letter's updates to the running world is a different job — it needs
// Location, TaskManager, Team and four building types — and it belongs in the
// layer that owns them rather than in the foundation.
//
// Moved out of ClientToServer::ProcessServerUpdates, which is why this is a free
// function: it never touched the endpoint it was a method on. See T12 in
// tasks/neuroncore-layering.yaml.

class ServerToClientLetter;

// Applies every update in an Update letter to the local game. The caller is
// expected to have offered the letter to ProcessServerLetters first; this
// handles the ones that letter-level handling did not claim.


namespace Species
{
  void ProcessServerUpdates(ServerToClientLetter* _letter);
} // namespace Species
