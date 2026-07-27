#pragma once

#include <stdlib.h>

namespace AALauncher {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::IO;
	using namespace System::Net;
	using namespace System::Net::Sockets;
	using namespace System::Text;

	/* Protocol matches AAServer's DiscoveryServerLoop (AAServer.cpp) and
	   ipxserver.h's DEFAULT_DISCOVERY_PORT/DISCOVERY_REQUEST_MAGIC/
	   DISCOVERY_REPLY_PREFIX -- kept as literals here rather than a shared
	   header since the launcher and server are separate repos/build
	   systems. */
	static const int DiscoveryPort = 21399;
	static String^ DiscoveryRequestMagic = "AASERVER_DISCOVER";
	static String^ DiscoveryReplyPrefix = "AASERVER_HERE:";

	// One row in the known-servers list -- ToString() is what ListBox
	// displays, since ListBox.Items shows each item's own ToString() by
	// default with no DisplayMember configured.
	private ref class ServerListEntry
	{
	public:
		String^ Name;
		String^ Host;
		String^ Port;

		ServerListEntry(String^ name, String^ host, String^ port)
			: Name(name), Host(host), Port(port)
		{
		}

		virtual String^ ToString() override
		{
			return Name + "  (" + Host + ":" + Port + ")";
		}
	};

	/// <summary>
	/// "Join Network Game" dialog: pick a saved or LAN-discovered server
	/// from a list, or type a host/IP + port directly. Saved favorites are
	/// stored in servers.txt (tab-separated name/host/port) alongside the
	/// launcher binary. LAN discovery broadcasts a probe and listens for
	/// replies on a background thread via BackgroundWorker, matching
	/// AAServer's DiscoveryServerLoop.
	/// </summary>
	public ref class NetworkIP : public System::Windows::Forms::Form
	{
	public:
		NetworkIP(String^ defaultPort)
		{
			InitializeComponent();
			System::Drawing::Icon ^ ico = gcnew System::Drawing::Icon(L"anaicon.ico");
			this->Icon = ico;

			iPortText->Text = defaultPort;
			LoadSavedServers();
			StartDiscovery();
		}

	protected:
		~NetworkIP()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::ListBox^ ServerListBox;
	private: System::Windows::Forms::Button^  iConnectButton;
	public: System::Windows::Forms::TextBox^  iIPNumberText;
	public: System::Windows::Forms::TextBox^  iPortText;
	private: System::Windows::Forms::TextBox^  iNameText;
	private: System::Windows::Forms::Button^  iSaveButton;
	private: System::Windows::Forms::Label^  _label;
	private: System::Windows::Forms::Label^  _label2;
	private: System::Windows::Forms::Label^  _label3;
	private: System::ComponentModel::BackgroundWorker^ discoveryWorker;
	protected:

	private:
		System::ComponentModel::Container ^components;

		void InitializeComponent(void)
		{
			this->ServerListBox = (gcnew System::Windows::Forms::ListBox());
			this->iConnectButton = (gcnew System::Windows::Forms::Button());
			this->iIPNumberText = (gcnew System::Windows::Forms::TextBox());
			this->iPortText = (gcnew System::Windows::Forms::TextBox());
			this->iNameText = (gcnew System::Windows::Forms::TextBox());
			this->iSaveButton = (gcnew System::Windows::Forms::Button());
			this->_label = (gcnew System::Windows::Forms::Label());
			this->_label2 = (gcnew System::Windows::Forms::Label());
			this->_label3 = (gcnew System::Windows::Forms::Label());
			this->SuspendLayout();
			//
			// _label (known servers)
			//
			this->_label->AutoSize = true;
			this->_label->Location = System::Drawing::Point(12, 9);
			this->_label->Name = L"_label";
			this->_label->Size = System::Drawing::Size(220, 13);
			this->_label->Text = L"Known servers (double-click to connect):";
			//
			// ServerListBox
			//
			this->ServerListBox->FormattingEnabled = true;
			this->ServerListBox->Location = System::Drawing::Point(15, 25);
			this->ServerListBox->Name = L"ServerListBox";
			this->ServerListBox->Size = System::Drawing::Size(390, 108);
			this->ServerListBox->TabIndex = 0;
			this->ServerListBox->SelectedIndexChanged += gcnew System::EventHandler(this, &NetworkIP::ServerListBox_SelectedIndexChanged);
			this->ServerListBox->DoubleClick += gcnew System::EventHandler(this, &NetworkIP::ServerListBox_DoubleClick);
			//
			// _label2 (manual entry)
			//
			this->_label2->AutoSize = true;
			this->_label2->Location = System::Drawing::Point(12, 141);
			this->_label2->Name = L"_label2";
			this->_label2->Size = System::Drawing::Size(180, 13);
			this->_label2->Text = L"Or enter a host/IP and port directly:";
			//
			// iIPNumberText
			//
			this->iIPNumberText->Location = System::Drawing::Point(15, 160);
			this->iIPNumberText->Name = L"iIPNumberText";
			this->iIPNumberText->Size = System::Drawing::Size(267, 20);
			this->iIPNumberText->TabIndex = 2;
			this->iIPNumberText->TextChanged += gcnew System::EventHandler(this, &NetworkIP::textBox1_TextChanged);
			//
			// iPortText
			//
			this->iPortText->Location = System::Drawing::Point(290, 160);
			this->iPortText->Name = L"iPortText";
			this->iPortText->Size = System::Drawing::Size(65, 20);
			this->iPortText->TabIndex = 3;
			this->iPortText->TextChanged += gcnew System::EventHandler(this, &NetworkIP::textBox1_TextChanged);
			//
			// _label3 (name for saving)
			//
			this->_label3->AutoSize = true;
			this->_label3->Location = System::Drawing::Point(12, 191);
			this->_label3->Name = L"_label3";
			this->_label3->Size = System::Drawing::Size(100, 13);
			this->_label3->Text = L"Name (to save):";
			//
			// iNameText
			//
			this->iNameText->Location = System::Drawing::Point(120, 188);
			this->iNameText->Name = L"iNameText";
			this->iNameText->Size = System::Drawing::Size(160, 20);
			this->iNameText->TabIndex = 4;
			//
			// iSaveButton
			//
			this->iSaveButton->Location = System::Drawing::Point(290, 186);
			this->iSaveButton->Name = L"iSaveButton";
			this->iSaveButton->Size = System::Drawing::Size(90, 24);
			this->iSaveButton->TabIndex = 5;
			this->iSaveButton->Text = L"Save";
			this->iSaveButton->UseVisualStyleBackColor = true;
			this->iSaveButton->Click += gcnew System::EventHandler(this, &NetworkIP::iSaveButton_Click);
			//
			// iConnectButton
			//
			this->iConnectButton->DialogResult = System::Windows::Forms::DialogResult::OK;
			this->iConnectButton->Enabled = false;
			this->iConnectButton->Location = System::Drawing::Point(290, 220);
			this->iConnectButton->Name = L"iConnectButton";
			this->iConnectButton->Size = System::Drawing::Size(90, 30);
			this->iConnectButton->TabIndex = 6;
			this->iConnectButton->Text = L"Connect";
			this->iConnectButton->UseVisualStyleBackColor = true;
			this->iConnectButton->Click += gcnew System::EventHandler(this, &NetworkIP::iConnectButton_Click);
			//
			// NetworkIP
			//
			this->AcceptButton = this->iConnectButton;
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(420, 262);
			this->Controls->Add(this->_label);
			this->Controls->Add(this->ServerListBox);
			this->Controls->Add(this->_label2);
			this->Controls->Add(this->iIPNumberText);
			this->Controls->Add(this->iPortText);
			this->Controls->Add(this->_label3);
			this->Controls->Add(this->iNameText);
			this->Controls->Add(this->iSaveButton);
			this->Controls->Add(this->iConnectButton);
			this->Name = L"NetworkIP";
			this->Text = L"AALauncher Network Connection";
			this->ResumeLayout(false);
			this->PerformLayout();
		}

	private:
		System::Void textBox1_TextChanged(System::Object^  sender, System::EventArgs^  e) {
			iConnectButton->Enabled = (iIPNumberText->Text->Trim()->Length > 0)
				&& (iPortText->Text->Trim()->Length > 0);
		}

		System::Void iConnectButton_Click(System::Object^  sender, System::EventArgs^  e) {
		}

		System::Void ServerListBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e) {
			ServerListEntry^ entry = dynamic_cast<ServerListEntry^>(ServerListBox->SelectedItem);
			if (entry == nullptr)
				return;
			iIPNumberText->Text = entry->Host;
			iPortText->Text = entry->Port;
		}

		System::Void ServerListBox_DoubleClick(System::Object^ sender, System::EventArgs^ e) {
			ServerListEntry^ entry = dynamic_cast<ServerListEntry^>(ServerListBox->SelectedItem);
			if (entry == nullptr)
				return;
			iIPNumberText->Text = entry->Host;
			iPortText->Text = entry->Port;
			this->DialogResult = System::Windows::Forms::DialogResult::OK;
			this->Close();
		}

		// servers.txt lives alongside the launcher binary -- one line per
		// entry, "name\thost\tport", '#' comments/blank lines ignored.
		String^ ServersFilePath() {
			return Application::StartupPath + "\\servers.txt";
		}

		void LoadSavedServers() {
			String^ path = ServersFilePath();
			if (!File::Exists(path))
				return;

			for each (String^ rawLine in File::ReadAllLines(path)) {
				String^ line = rawLine->Trim();
				if (line->Length == 0 || line->StartsWith("#"))
					continue;
				array<String^>^ parts = line->Split(gcnew array<Char>{'\t'});
				if (parts->Length < 3)
					continue;
				ServerListBox->Items->Add(gcnew ServerListEntry(parts[0]->Trim(), parts[1]->Trim(), parts[2]->Trim()));
			}
		}

		System::Void iSaveButton_Click(System::Object^ sender, System::EventArgs^ e) {
			String^ host = iIPNumberText->Text->Trim();
			String^ port = iPortText->Text->Trim();
			if (host->Length == 0 || port->Length == 0)
				return;

			String^ name = iNameText->Text->Trim();
			if (name->Length == 0)
				name = host;

			String^ line = name + "\t" + host + "\t" + port + "\r\n";
			File::AppendAllText(ServersFilePath(), line);
			ServerListBox->Items->Add(gcnew ServerListEntry(name, host, port));
		}

		// Broadcasts a LAN discovery probe and adds each reply to the list
		// as it arrives, without blocking the UI thread -- BackgroundWorker
		// marshals ProgressChanged back onto the UI thread automatically.
		void StartDiscovery() {
			discoveryWorker = gcnew BackgroundWorker();
			discoveryWorker->WorkerReportsProgress = true;
			discoveryWorker->DoWork += gcnew DoWorkEventHandler(this, &NetworkIP::DiscoveryWorker_DoWork);
			discoveryWorker->ProgressChanged += gcnew ProgressChangedEventHandler(this, &NetworkIP::DiscoveryWorker_ProgressChanged);
			discoveryWorker->RunWorkerAsync();
		}

		System::Void DiscoveryWorker_DoWork(System::Object^ sender, System::ComponentModel::DoWorkEventArgs^ e) {
			BackgroundWorker^ worker = (BackgroundWorker^)sender;
			UdpClient^ client = gcnew UdpClient();
			try {
				client->EnableBroadcast = true;
				client->Client->ReceiveTimeout = 200;

				array<Byte>^ reqBytes = Encoding::ASCII->GetBytes(DiscoveryRequestMagic);
				client->Send(reqBytes, reqBytes->Length, gcnew IPEndPoint(IPAddress::Broadcast, DiscoveryPort));

				DateTime deadline = DateTime::Now.AddMilliseconds(1500);
				IPEndPoint^ remoteEP = gcnew IPEndPoint(IPAddress::Any, 0);
				while (DateTime::Now < deadline) {
					try {
						array<Byte>^ data = client->Receive(remoteEP);
						String^ text = Encoding::ASCII->GetString(data);
						if (text->StartsWith(DiscoveryReplyPrefix)) {
							String^ rest = text->Substring(DiscoveryReplyPrefix->Length);
							array<String^>^ parts = rest->Split(gcnew array<Char>{':'});
							if (parts->Length >= 2) {
								ServerListEntry^ entry = gcnew ServerListEntry(parts[1] + " [LAN]", remoteEP->Address->ToString(), parts[0]);
								worker->ReportProgress(0, entry);
							}
						}
					} catch (Exception^) {
						// Receive timeout or transient error -- keep polling until deadline.
					}
				}
			} finally {
				client->Close();
			}
		}

		System::Void DiscoveryWorker_ProgressChanged(System::Object^ sender, System::ComponentModel::ProgressChangedEventArgs^ e) {
			ServerListEntry^ entry = (ServerListEntry^)e->UserState;
			ServerListBox->Items->Add(entry);
		}
	};
}
