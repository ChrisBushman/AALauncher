#pragma once

#include "NetworkIP.h"
#include "ScriptCompilerForm.h"
#include <stdlib.h>
#include <Windows.h>
#include <ShellAPI.h>

namespace AALauncher {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
    using namespace System::Runtime::InteropServices;
    using namespace System::IO;

	/// <summary>
	/// Summary for Form1
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
            System::Drawing::Icon ^ ico = gcnew System::Drawing::Icon(L"anaicon.ico");
            this->Icon = ico;
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}
    private: System::Windows::Forms::Panel^  panel1;
    protected: 

    private: System::Windows::Forms::Button^  PlayNetworkGame;
    private: System::Windows::Forms::Button^  PlaySinglePlayer;


    private: System::Windows::Forms::Button^  StartServerButton;
    private: System::Windows::Forms::Label^   PortLabel;
    private: System::Windows::Forms::TextBox^ PortTextBox;
    private: System::Windows::Forms::Button^  ScriptCompilerButton;
    private: System::Windows::Forms::Button^  ExitButton;

    private: System::Windows::Forms::WebBrowser^  webBrowser1;

    private: ScriptCompilerForm^ scriptCompilerForm;


	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
            this->panel1 = (gcnew System::Windows::Forms::Panel());
            this->PlayNetworkGame = (gcnew System::Windows::Forms::Button());
            this->PlaySinglePlayer = (gcnew System::Windows::Forms::Button());
            this->StartServerButton = (gcnew System::Windows::Forms::Button());
            this->PortLabel = (gcnew System::Windows::Forms::Label());
            this->PortTextBox = (gcnew System::Windows::Forms::TextBox());
            this->ScriptCompilerButton = (gcnew System::Windows::Forms::Button());
            this->ExitButton = (gcnew System::Windows::Forms::Button());
            this->webBrowser1 = (gcnew System::Windows::Forms::WebBrowser());
            this->panel1->SuspendLayout();
            this->SuspendLayout();
            //
            // panel1
            //
            this->panel1->Controls->Add(this->PlayNetworkGame);
            this->panel1->Controls->Add(this->PlaySinglePlayer);
            this->panel1->Controls->Add(this->StartServerButton);
            this->panel1->Controls->Add(this->PortLabel);
            this->panel1->Controls->Add(this->PortTextBox);
            this->panel1->Controls->Add(this->ScriptCompilerButton);
            this->panel1->Controls->Add(this->ExitButton);
            this->panel1->Dock = System::Windows::Forms::DockStyle::Bottom;
            this->panel1->Location = System::Drawing::Point(0, 462);
            this->panel1->Name = L"panel1";
            this->panel1->Size = System::Drawing::Size(1049, 160);
            this->panel1->TabIndex = 0;
            // 
            // PlayNetworkGame
            // 
            this->PlayNetworkGame->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
                static_cast<System::Byte>(0)));
            this->PlayNetworkGame->Location = System::Drawing::Point(406, 21);
            this->PlayNetworkGame->Name = L"PlayNetworkGame";
            this->PlayNetworkGame->Size = System::Drawing::Size(234, 58);
            this->PlayNetworkGame->TabIndex = 2;
            this->PlayNetworkGame->Text = L"Play &Network Game";
            this->PlayNetworkGame->UseVisualStyleBackColor = true;
            this->PlayNetworkGame->Click += gcnew System::EventHandler(this, &Form1::button3_Click);
            // 
            // PlaySinglePlayer
            // 
            this->PlaySinglePlayer->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
                static_cast<System::Byte>(0)));
            this->PlaySinglePlayer->Location = System::Drawing::Point(665, 21);
            this->PlaySinglePlayer->Name = L"PlaySinglePlayer";
            this->PlaySinglePlayer->Size = System::Drawing::Size(234, 58);
            this->PlaySinglePlayer->TabIndex = 0;
            this->PlaySinglePlayer->Text = L"&Play Single Player";
            this->PlaySinglePlayer->UseVisualStyleBackColor = true;
            this->PlaySinglePlayer->Click += gcnew System::EventHandler(this, &Form1::button2_Click);
            // 
            // StartServerButton
            // 
            this->StartServerButton->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point, 
                static_cast<System::Byte>(0)));
            this->StartServerButton->Location = System::Drawing::Point(148, 21);
            this->StartServerButton->Name = L"StartServerButton";
            this->StartServerButton->Size = System::Drawing::Size(234, 58);
            this->StartServerButton->TabIndex = 1;
            this->StartServerButton->Text = L"Start A&&A &Server";
            this->StartServerButton->UseVisualStyleBackColor = true;
            this->StartServerButton->Click += gcnew System::EventHandler(this, &Form1::button1_Click);
            //
            // PortLabel
            //
            this->PortLabel->AutoSize = true;
            this->PortLabel->Location = System::Drawing::Point(148, 103);
            this->PortLabel->Name = L"PortLabel";
            this->PortLabel->Size = System::Drawing::Size(70, 13);
            this->PortLabel->Text = L"Server Port:";
            //
            // PortTextBox
            //
            this->PortTextBox->Location = System::Drawing::Point(228, 100);
            this->PortTextBox->Name = L"PortTextBox";
            this->PortTextBox->Size = System::Drawing::Size(70, 20);
            this->PortTextBox->TabIndex = 3;
            this->PortTextBox->Text = L"21300";
            //
            // ScriptCompilerButton
            //
            this->ScriptCompilerButton->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
                static_cast<System::Byte>(0)));
            this->ScriptCompilerButton->Location = System::Drawing::Point(406, 92);
            this->ScriptCompilerButton->Name = L"ScriptCompilerButton";
            this->ScriptCompilerButton->Size = System::Drawing::Size(234, 40);
            this->ScriptCompilerButton->TabIndex = 4;
            this->ScriptCompilerButton->Text = L"Script Compiler";
            this->ScriptCompilerButton->UseVisualStyleBackColor = true;
            this->ScriptCompilerButton->Click += gcnew System::EventHandler(this, &Form1::scriptCompilerButton_Click);
            //
            // ExitButton
            //
            this->ExitButton->Font = (gcnew System::Drawing::Font(L"Microsoft Sans Serif", 8.25F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
                static_cast<System::Byte>(0)));
            this->ExitButton->Location = System::Drawing::Point(665, 92);
            this->ExitButton->Name = L"ExitButton";
            this->ExitButton->Size = System::Drawing::Size(234, 40);
            this->ExitButton->TabIndex = 5;
            this->ExitButton->Text = L"Exit";
            this->ExitButton->UseVisualStyleBackColor = true;
            this->ExitButton->Click += gcnew System::EventHandler(this, &Form1::exitButton_Click);
            //
            // webBrowser1
            //
            this->webBrowser1->Dock = System::Windows::Forms::DockStyle::Fill;
            this->webBrowser1->Location = System::Drawing::Point(0, 0);
            this->webBrowser1->MinimumSize = System::Drawing::Size(20, 20);
            this->webBrowser1->Name = L"webBrowser1";
            this->webBrowser1->ScriptErrorsSuppressed = true;
            this->webBrowser1->Size = System::Drawing::Size(1049, 462);
            this->webBrowser1->TabIndex = 1;
            this->webBrowser1->Url = (gcnew System::Uri(L"http://www.amuletsandarmor.com/index.htm\?launcher=1&classic=1", System::UriKind::Absolute));
            // 
            // Form1
            // 
            this->AcceptButton = this->PlaySinglePlayer;
            this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->ClientSize = System::Drawing::Size(1049, 622);
            this->Controls->Add(this->webBrowser1);
            this->Controls->Add(this->panel1);
            this->Name = L"Form1";
            this->Text = L"Amulets & Armor Windows Launcher v1.00";
            this->Load += gcnew System::EventHandler(this, &Form1::Form1_Load);
            this->Shown += gcnew System::EventHandler(this, &Form1::Form1_Shown);
            this->panel1->ResumeLayout(false);
            this->ResumeLayout(false);

        }
#pragma endregion
    private: System::Void button1_Click(System::Object^  sender, System::EventArgs^  e) {
             IntPtr p = Marshal::StringToHGlobalAnsi(PortTextBox->Text->Trim());
             const char* pAnsi = static_cast<const char*>(p.ToPointer());
             HINSTANCE i = ShellExecute(
                NULL,
                "open",
                "AAServer.exe",
                pAnsi,
                NULL,
                SW_SHOWNOACTIVATE
            );
            Marshal::FreeHGlobal(p);
            if ((int)i < 32) {
                MessageBox::Show("Failed to start server!");
             } else {
             }

             }
private: System::Void label2_Click(System::Object^  sender, System::EventArgs^  e) {
         }
private: System::Void button3_Click(System::Object^  sender, System::EventArgs^  e) {
             NetworkIP ^ f = gcnew NetworkIP(PortTextBox->Text->Trim());
             if (f->ShowDialog(this) == System::Windows::Forms::DialogResult::OK) {
                String^ params = f->iIPNumberText->Text + " " + f->iPortText->Text->Trim();
                IntPtr p = Marshal::StringToHGlobalAnsi(params);
                const char* pAnsi = static_cast<const char*>(p.ToPointer());

                 // Launch connecting to the server
                 HINSTANCE i = ShellExecute(
                    NULL,
                    "open",
                    "AA.exe",
                    pAnsi,
                    NULL,
                    SW_SHOWNORMAL
                );
                // use pAnsi
                Marshal::FreeHGlobal(p);
                if ((int)i < 32) {
                    MessageBox::Show("Failed to start application!");
                 } else {
                    Close();
                 }
             }
             delete f;
         }
private: System::Void scriptCompilerButton_Click(System::Object^ sender, System::EventArgs^ e) {
             if (scriptCompilerForm == nullptr || scriptCompilerForm->IsDisposed) {
                 scriptCompilerForm = gcnew ScriptCompilerForm(
                     System::IO::Path::Combine(Application::StartupPath, "AAScriptCompiler.exe"));
             }
             scriptCompilerForm->Show();
             scriptCompilerForm->Activate();
         }
private: System::Void exitButton_Click(System::Object^ sender, System::EventArgs^ e) {
             Application::Exit();
         }

private: System::Void button2_Click(System::Object^  sender, System::EventArgs^  e) {
             // Launch as single player
             //system("WinAA.exe");
             HINSTANCE i = ShellExecute(
                NULL,
                "open",
                "AA.exe",
                NULL,
                NULL,
                SW_SHOW
            );
            if ((int)i < 32) {
                MessageBox::Show("Failed to start application!");
             } else {
                Close();
             }
         }
private: System::Void Form1_Load(System::Object^  sender, System::EventArgs^  e) {
            this->webBrowser1->Refresh();
         }
private: System::Void Form1_Shown(System::Object^  sender, System::EventArgs^  e) {
            this->webBrowser1->Refresh();
         }
};
}

