#include "hop/Snooper.h"

#include "hop/Options.h"
#include "hop/Viewer.h"
#include "hop/ProfilerView.h"
#include "hop/ModalWindow.h"

#include "imgui/imgui.h"

#include <stdio.h>
#include <algorithm>

namespace hop
{

Snooper::Snooper() : _selectedConnection( -1 ), _windowOpen( false )
{
   snprintf( _addressStr, sizeof( _addressStr ), "%s", hop::options::lastAddressUsed() );
   snprintf( _portStr, sizeof( _portStr ), hop::options::lastPortUsed() );
}

void Snooper::enable()
{
   _windowOpen = true;
}

void Snooper::draw( Viewer* viewer )
{
   if( _windowOpen )
   {
      ImGui::SetNextWindowPos( ImVec2( 30.0f, 30.0f ), ImGuiCond_Once );
      ImGui::SetNextWindowSize( ImVec2( 400.0f, 200.0f ), ImGuiCond_Once );

      ImGui::Begin( "Remote Connections", &_windowOpen, ImGuiWindowFlags_NoScrollWithMouse );
      ImGui::Text( "Test" );
      ImGui::InputText( "Address", _addressStr, sizeof( _addressStr ) );
      ImGui::InputText( "Port", _portStr, sizeof( _portStr ), ImGuiInputTextFlags_CharsDecimal );

      static ConnectionState state = NOT_CONNECTED;
      if( ImGui::Button( "Scan" ) )
      {
         if( _addressStr[0] != '\0' && _portStr[0] != '\0')
            hop::options::setLastAddressUsed( _addressStr, _portStr );

         _pending_connections.clear();
         if( _pending_connections.empty() )
         {
            uint32_t port = atoi( _portStr );
            uint32_t endPort = port + 16;
            for( ; port < endPort; port++ )
            {
               /* Check if we already have this connection opened */
               bool exists = false;
               for( const auto& p : _profilers )
               {
                  const NetworkConnection* cn = p->data().networkConnection();
                  uint32_t cn_port = atoi( cn->_portStr );
                  if( strcmp( cn->_addressStr, _addressStr ) == 0 && cn_port == port )
                  {
                     exists = true;
                     break;
                  }
               }

               if( !exists )
               {
                  int profCount = viewer->profilerCount();
                  for( int i = 0; i < profCount; i++ )
                  {
                     const ProfilerView* pview   = viewer->getProfiler( i );
                     const NetworkConnection* cn = pview->data().networkConnection();
                     if( !cn )
                        continue;
                     uint32_t cn_port            = atoi( cn->_portStr );
                     if( strcmp( cn->_addressStr, _addressStr ) == 0 && cn_port == port )
                     {
                        exists = true;
                        break;
                     }
                  }
               }

               if( exists ) continue;

               NetworkConnection nc = {};
               memcpy( nc._addressStr, _addressStr, sizeof( _addressStr ) );
               snprintf( nc._portStr, sizeof( _portStr ), "%u", port );
               state = nc.openConnection( true );
               if( state == CANNOT_RESOLVE_ADDR || state == CANNOT_CONNECT_TO_SERVER ) break;
               _pending_connections.emplace_back( new NetworkConnection( std::move( nc ) ) );
            }
         }

         for( auto &pcn : _pending_connections)
         {
            if( pcn->openConnection( true ) == CONNECTED )
            {
               _profilers.push_back ( std::make_unique<hop::ProfilerView>( std::move( pcn ) ) );
            }
         }

         _pending_connections.erase(
             std::remove_if(
                 _pending_connections.begin(),
                 _pending_connections.end(),
                 []( const std::unique_ptr<NetworkConnection>& conn ) { return conn.get() == nullptr; } ),
             _pending_connections.end() );
      }

      if( state == CANNOT_RESOLVE_ADDR )
      {
         ImGui::SameLine();
         ImGui::TextColored( ImVec4(1.0, 0.0, 0.0, 1.0), "Invalid Address" );
      }

      char label[256];
      for( int32_t i = 0; i < (int32_t)_profilers.size(); i++ )
      {
         const Profiler& prof       = _profilers[i]->data();
         const NetworkConnection *c = prof.networkConnection();
         if( c->status() == NetworkConnection::Status::ALIVE )
         {
            ImGui::PushID( i );
            int pid;
            const char *name = prof.nameAndPID(&pid);
            snprintf( label, sizeof( label ), "[%u - %s:%s] %s (%d)", i, c->_addressStr, c->_portStr, name, pid );
            if( ImGui::Selectable( label, i == _selectedConnection ) )
            {
               _selectedConnection = i;
            }
            ImGui::PopID();
         }
      }

      if (ImGui::Button( "Connect" ) && _selectedConnection >= 0)
      {
         viewer->addProfiler( std::move( _profilers[_selectedConnection] ), true );
         _profilers.erase( _profilers.begin() + _selectedConnection );
         _selectedConnection = _selectedConnection > 0 ? _selectedConnection - 1 : 0;
      }

      ImGui::End();
   }
}

}  // namespace hop
