#include "PanelStreamer.h"

#include "app/widgets/GenerateImGuiWidgets.h"

#include "imgui.h"

namespace ospray {
  namespace streamer_plugin {

  PanelStreamer::PanelStreamer(std::shared_ptr<StudioContext> _context, std::string _panelName)
      : Panel(_panelName.c_str(), _context)
      , panelName(_panelName)
  {}

  void PanelStreamer::buildUI(void *ImGuiCtx)
  {
    // Need to set ImGuiContext in *this* address space
    ImGui::SetCurrentContext((ImGuiContext *)ImGuiCtx);
    ImGui::OpenPopup(panelName.c_str());

    if (ImGui::BeginPopupModal(
            panelName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("%s", "The Streamer Plugin receives data from Gesture Tracking Server.");
      ImGui::Separator();

      ImGui::Text("Status: \n%s", status.c_str());
      ImGui::Separator();

      if (ImGui::Button("Connect")) {
        // Initialize socket.
        tcpSocket = new TCPSocket([](int errorCode, std::string errorMessage){
            std::cout << "Socket creation error:" << errorCode << " : " << errorMessage << std::endl;
        });

        // Start receiving from the host.
        tcpSocket->onMessageReceived = [&](std::string message) {
            std::cout << "Message from the Server: " << message << std::endl;
            status = message;
        };
        
        // On socket closed:
        tcpSocket->onSocketClosed = [](int errorCode){
            std::cout << "Connection closed: " << errorCode << std::endl;
        };

        // Connect to the host.
        tcpSocket->Connect("localhost", 8888, [&] {
            std::cout << "Connected to the server successfully." << std::endl;
        },
        [](int errorCode, std::string errorMessage){
            // CONNECTION FAILED
            std::cout << errorCode << " : " << errorMessage << std::endl;
        });
      }
      if (ImGui::Button("Disconnect")) {
        tcpSocket->Close();
      }

      ImGui::Separator();
      if (ImGui::Button("Close")) {
        setShown(false);
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
  }

  }  // namespace streamer_plugin
}  // namespace ospray
