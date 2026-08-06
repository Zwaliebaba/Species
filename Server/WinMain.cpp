#include "pch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HiResTime.h"
#include "Preferences.h"
#include "Profiler.h"
#include "ProtocolLimits.h"
#include "Server.h"

// The headless authoritative host.
//
// This binary is the point of the layering work: it links NeuronCore and
// NeuronServer and nothing else, so if it runs, the foundation really is free of
// the game client rather than merely tidy. See AGENTS.md, Definition of done.
//
// It is not the world server. It sequences whatever clients send it, exactly as
// the in-process host always did, and holds no simulation of its own. Advance()
// stamps every outgoing letter with a sequence id whether or not anyone is
// listening, which is what makes "advances sequence ids with no client attached"
// something you can watch rather than something to take on trust.
//
// Naming: the file is WinMain.cpp for historical reasons and its entry point is
// main() — Server.vcxproj builds a console subsystem application.

namespace
{
  const char* const PREFERENCES_FILE = "server-preferences.txt";

  // How often to report progress when running unbounded, in ticks. At
  // SERVER_ADVANCE_PERIOD this is roughly every ten seconds.
  const int REPORT_EVERY = 100;

  // --ticks N runs N ticks and exits, so the sequence-id claim can be checked by
  // a script. Absent, the host runs until it is killed, which is what a server
  // does.
  int ParseTickLimit(int _argc, char* _argv[])
  {
    for (int i = 1; i < _argc - 1; ++i)
    {
      if (strcmp(_argv[i], "--ticks") == 0)
        return atoi(_argv[i + 1]);
    }

    return -1;
  }
} // namespace

int main(int argc, char* argv[])
{
  const int tickLimit = ParseTickLimit(argc, argv);

  // Required before GetHighResTime means anything. Until this runs the tick
  // interval is 1.0, so the "seconds" it returns are raw performance-counter
  // ticks — millions of them — and every sleep below computes as negative and is
  // skipped. The host then spins as fast as the CPU allows instead of ticking at
  // SERVER_ADVANCE_FREQ. Species/Main.cpp was the only caller before this.
  InitialiseHighResTime();

  // Server::Advance reads RecordDemo, so the store has to exist. There being no
  // preferences file beside a fresh server is fine: PrefsManager falls back to
  // its built-in defaults, and with no defaults provider installed it does not
  // go looking for the client's GameData either.
  g_prefsManager = new PrefsManager(PREFERENCES_FILE);

  // PROFILER_ENABLED is on by default and START_PROFILE dereferences whatever it
  // is handed, so this is required rather than optional.
  auto profiler = new Profiler();

  Server server;
  server.Initialise(profiler);

  printf("Species server listening on port 4000, ticking at %.0f Hz.\n", SERVER_ADVANCE_FREQ);
  if (tickLimit >= 0)
    printf("Stopping after %d tick(s).\n", tickLimit);

  double nextAdvance = GetHighResTime();

  for (int tick = 0; tickLimit < 0 || tick < tickLimit; ++tick)
  {
    server.Advance();

    if (tickLimit < 0 && tick % REPORT_EVERY == 0)
      printf("tick %d, sequence id %d\n", tick, server.m_sequenceId);

    nextAdvance += SERVER_ADVANCE_PERIOD;
    const double waitFor = nextAdvance - GetHighResTime();
    if (waitFor > 0.0)
      Sleep(static_cast<DWORD>(waitFor * 1000.0));
    else
      nextAdvance = GetHighResTime(); // Fell behind; do not try to catch up
  }

  printf("Stopped at sequence id %d.\n", server.m_sequenceId);

  // There is no listen thread to outlive this any more — network-transport T4
  // replaced it with a socket the Server owns and polls, so the shutdown gap
  // that used to be recorded here closed with it.
  delete profiler;
  delete g_prefsManager;
  g_prefsManager = nullptr;

  return 0;
}
