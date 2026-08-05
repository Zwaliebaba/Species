#include "pch.h"


#include <stdio.h>

#include "TextRenderer.h"
#include "Profiler.h"

#include "Server.h"
#include "ClientToServer.h"

#include "GameTime.h"

#include "NetworkWindow.h"
#include "AppState.h"


namespace Species
{
  NetworkWindow::NetworkWindow(char const* name)
    : SpeciesWindow(name)
  {
  }


  void NetworkWindow::Render(bool hasFocus)
  {
    SpeciesWindow::Render( hasFocus );

    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

    //
    // Render some Networking stats

    if( g_server )
    {
#ifdef PROFILER_ENABLED
//        g_editorFont.DrawText2D( m_x + 10, m_y + 120, DEF_FONT_SIZE,
//			"Server SEND  : %4.0f bytes", g_profiler->GetTotalTime("Server Send") );
//        g_editorFont.DrawText2D( m_x + 10, m_y + 135, DEF_FONT_SIZE,
//			"Server RECV  : %4.0f bytes", g_profiler->GetTotalTime("Server Receive") );
#endif // PROFILER_ENABLED
      g_editorFont.DrawText2D(m_x + 10, m_y + 30, DEF_FONT_SIZE, "SERVER SeqID : {}", g_server->m_sequenceId);

      int diff = g_server->m_sequenceId - g_lastProcessedSequenceId;
      g_editorFont.DrawText2D(m_x + 10, m_y + 60, DEF_FONT_SIZE, "Diff         : {}", diff);
    }

#ifdef PROFILER_ENABLED
//    g_editorFont.DrawText2D( m_x + 10, m_y + 160, DEF_FONT_SIZE,
//		"Client SEND  : %4.0f bytes", g_profiler->GetTotalTime("Client Send") );
//    g_editorFont.DrawText2D( m_x + 10, m_y + 175, DEF_FONT_SIZE,
//		"Client RECV  : %4.0f bytes", g_profiler->GetTotalTime("Client Receive") );
#endif // PROFILER_ENABLED
    g_editorFont.DrawText2D(m_x + 10, m_y + 45, DEF_FONT_SIZE, "CLIENT SeqID : {}", g_lastProcessedSequenceId);

    g_editorFont.DrawText2D(m_x + 10, m_y + 80, DEF_FONT_SIZE, "Inbox: {}", static_cast<int>(g_clientToServer->m_inbox.size()));

    int nextSeqId = g_clientToServer->GetNextLetterSeqID();
    g_editorFont.DrawText2D(m_x + 10, m_y + 96, DEF_FONT_SIZE, "First Letter SeqID: {}", nextSeqId);
  }
} // namespace Species
