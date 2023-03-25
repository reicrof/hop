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
   snprintf( _portStr, sizeof( _portStr ), "12345" );
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
         if( strlen(_addressStr) > 0 )
           hop::options::setLastAddressUsed (_addressStr);

         _pending_connections.clear();
         if( _pending_connections.empty() )
         {
            uint32_t port = atoi( _portStr );
            uint32_t endPort = port + 16;
            for( ; port < endPort; port++ )
            {
               /* Check if we already have this connection opened */
               bool exists = false;
               for( const auto& cn : _connections )
               {
                  uint32_t cn_port = atoi( cn->_portStr );
                  if( strcmp( cn->_addressStr, _addressStr ) == 0 && cn_port == port )
                  {
                     exists = true;
                     break;
                  }
               }

               if( exists ) continue;

               NetworkConnection nc = {};
               memcpy( nc._addressStr, _addressStr, sizeof( _addressStr ) );
               snprintf( nc._portStr, sizeof( _portStr ), "%u", port );
               state = nc.openConnection( true );
               // nc.openConnection( true );
               if( state == CANNOT_RESOLVE_ADDR || state == CANNOT_CONNECT_TO_SERVER ) break;
               _pending_connections.emplace_back( new NetworkConnection( std::move( nc ) ) );
            }
         }

         for( auto &pcn : _pending_connections)
         {
            if( pcn->openConnection( true ) == CONNECTED )
               _connections.emplace_back (pcn.release());
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
      for( int32_t i = 0; i < (int32_t)_connections.size(); i++ )
      {
         const auto& c = _connections[i];
         if( c->status() == NetworkConnection::Status::ALIVE )
         {
            ImGui::PushID( i );
            snprintf( label, sizeof( label ), "[%u] %s:%s", i, c->_addressStr, c->_portStr );
            if( ImGui::Selectable( label, i == _selectedConnection ) )
            {
               _selectedConnection = i;
            }
            ImGui::PopID();
         }
      }

      if (ImGui::Button( "Connect" ) && _selectedConnection >= 0)
      {
         auto profiler = std::make_unique<hop::ProfilerView>( *_connections[_selectedConnection] );
         viewer->addProfiler( std::move( profiler ), true );

      }

      ImGui::End();
   }
}

}  // namespace hop
