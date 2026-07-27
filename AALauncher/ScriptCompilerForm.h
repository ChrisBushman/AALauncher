#pragma once

namespace AALauncher {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::IO;
	using namespace System::Diagnostics;

	/// <summary>
	/// Standalone (non-modal) window wrapping AAScriptCompiler's
	/// "SC &lt;script&gt; &lt;output&gt;" CLI -- lets a content author
	/// compile a .SRC/.SRP without leaving the launcher.
	/// </summary>
	public ref class ScriptCompilerForm : public System::Windows::Forms::Form
	{
	public:
		ScriptCompilerForm(String^ compilerPath)
			: m_compilerPath(compilerPath)
		{
			InitializeComponent();
		}

	protected:
		~ScriptCompilerForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private:
		String^ m_compilerPath;
		Diagnostics::Process^ m_process;

		System::Windows::Forms::TextBox^ ScriptPathText;
		System::Windows::Forms::TextBox^ OutputPathText;
		System::Windows::Forms::Button^  BrowseScriptButton;
		System::Windows::Forms::Button^  BrowseOutputButton;
		System::Windows::Forms::Button^  CompileButton;
		System::Windows::Forms::TextBox^ LogTextBox;
		System::ComponentModel::Container^ components;

		void InitializeComponent(void)
		{
			this->ScriptPathText = (gcnew System::Windows::Forms::TextBox());
			this->OutputPathText = (gcnew System::Windows::Forms::TextBox());
			this->BrowseScriptButton = (gcnew System::Windows::Forms::Button());
			this->BrowseOutputButton = (gcnew System::Windows::Forms::Button());
			this->CompileButton = (gcnew System::Windows::Forms::Button());
			this->LogTextBox = (gcnew System::Windows::Forms::TextBox());
			this->SuspendLayout();

			Label^ scriptLabel = gcnew Label();
			scriptLabel->AutoSize = true;
			scriptLabel->Location = System::Drawing::Point(12, 15);
			scriptLabel->Text = L"Script file:";

			this->ScriptPathText->Location = System::Drawing::Point(90, 12);
			this->ScriptPathText->Size = System::Drawing::Size(400, 20);

			this->BrowseScriptButton->Location = System::Drawing::Point(496, 10);
			this->BrowseScriptButton->Size = System::Drawing::Size(90, 24);
			this->BrowseScriptButton->Text = L"Browse...";
			this->BrowseScriptButton->UseVisualStyleBackColor = true;
			this->BrowseScriptButton->Click += gcnew System::EventHandler(this, &ScriptCompilerForm::BrowseScriptButton_Click);

			Label^ outputLabel = gcnew Label();
			outputLabel->AutoSize = true;
			outputLabel->Location = System::Drawing::Point(12, 45);
			outputLabel->Text = L"Output file:";

			this->OutputPathText->Location = System::Drawing::Point(90, 42);
			this->OutputPathText->Size = System::Drawing::Size(400, 20);

			this->BrowseOutputButton->Location = System::Drawing::Point(496, 40);
			this->BrowseOutputButton->Size = System::Drawing::Size(90, 24);
			this->BrowseOutputButton->Text = L"Browse...";
			this->BrowseOutputButton->UseVisualStyleBackColor = true;
			this->BrowseOutputButton->Click += gcnew System::EventHandler(this, &ScriptCompilerForm::BrowseOutputButton_Click);

			this->CompileButton->Location = System::Drawing::Point(12, 75);
			this->CompileButton->Size = System::Drawing::Size(574, 30);
			this->CompileButton->Text = L"Compile";
			this->CompileButton->UseVisualStyleBackColor = true;
			this->CompileButton->Click += gcnew System::EventHandler(this, &ScriptCompilerForm::CompileButton_Click);

			this->LogTextBox->Location = System::Drawing::Point(12, 115);
			this->LogTextBox->Size = System::Drawing::Size(574, 320);
			this->LogTextBox->Multiline = true;
			this->LogTextBox->ReadOnly = true;
			this->LogTextBox->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
			this->LogTextBox->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(
				System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom |
				System::Windows::Forms::AnchorStyles::Left | System::Windows::Forms::AnchorStyles::Right);

			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(600, 447);
			this->Controls->Add(scriptLabel);
			this->Controls->Add(this->ScriptPathText);
			this->Controls->Add(this->BrowseScriptButton);
			this->Controls->Add(outputLabel);
			this->Controls->Add(this->OutputPathText);
			this->Controls->Add(this->BrowseOutputButton);
			this->Controls->Add(this->CompileButton);
			this->Controls->Add(this->LogTextBox);
			this->Name = L"ScriptCompilerForm";
			this->Text = L"Amulets & Armor Script Compiler";
			this->ResumeLayout(false);
			this->PerformLayout();
		}

		System::Void BrowseScriptButton_Click(System::Object^ sender, System::EventArgs^ e) {
			OpenFileDialog^ dlg = gcnew OpenFileDialog();
			dlg->Filter = "Script files (*.SRC;*.SRP)|*.SRC;*.SRP|All files (*.*)|*.*";
			if (dlg->ShowDialog(this) == System::Windows::Forms::DialogResult::OK) {
				ScriptPathText->Text = dlg->FileName;
				if (OutputPathText->Text->Trim()->Length == 0) {
					OutputPathText->Text = Path::Combine(Path::GetDirectoryName(dlg->FileName),
						Path::GetFileNameWithoutExtension(dlg->FileName) + ".OUT");
				}
			}
		}

		System::Void BrowseOutputButton_Click(System::Object^ sender, System::EventArgs^ e) {
			SaveFileDialog^ dlg = gcnew SaveFileDialog();
			dlg->FileName = OutputPathText->Text;
			if (dlg->ShowDialog(this) == System::Windows::Forms::DialogResult::OK) {
				OutputPathText->Text = dlg->FileName;
			}
		}

		System::Void CompileButton_Click(System::Object^ sender, System::EventArgs^ e) {
			String^ scriptPath = ScriptPathText->Text->Trim();
			String^ outputPath = OutputPathText->Text->Trim();
			if (scriptPath->Length == 0 || outputPath->Length == 0) {
				MessageBox::Show("Please choose both a script file and an output file.", "Script Compiler");
				return;
			}
			if (!File::Exists(m_compilerPath)) {
				MessageBox::Show("Compiler not found: " + m_compilerPath, "Script Compiler");
				return;
			}

			LogTextBox->Clear();
			CompileButton->Enabled = false;

			m_process = gcnew Diagnostics::Process();
			m_process->StartInfo->FileName = m_compilerPath;
			m_process->StartInfo->Arguments = "\"" + scriptPath + "\" \"" + outputPath + "\"";
			m_process->StartInfo->UseShellExecute = false;
			m_process->StartInfo->RedirectStandardOutput = true;
			m_process->StartInfo->RedirectStandardError = true;
			m_process->StartInfo->CreateNoWindow = true;
			m_process->EnableRaisingEvents = true;
			m_process->OutputDataReceived += gcnew DataReceivedEventHandler(this, &ScriptCompilerForm::Process_DataReceived);
			m_process->ErrorDataReceived += gcnew DataReceivedEventHandler(this, &ScriptCompilerForm::Process_DataReceived);
			m_process->Exited += gcnew System::EventHandler(this, &ScriptCompilerForm::Process_Exited);

			try {
				m_process->Start();
				m_process->BeginOutputReadLine();
				m_process->BeginErrorReadLine();
			} catch (Exception^ ex) {
				MessageBox::Show("Failed to launch compiler: " + ex->Message, "Script Compiler");
				CompileButton->Enabled = true;
			}
		}

		void AppendLog(String^ text) {
			if (LogTextBox->InvokeRequired) {
				LogTextBox->Invoke(gcnew Action<String^>(this, &ScriptCompilerForm::AppendLog), text);
				return;
			}
			LogTextBox->AppendText(text + "\r\n");
		}

		System::Void Process_DataReceived(Object^ sender, DataReceivedEventArgs^ e) {
			if (e->Data != nullptr)
				AppendLog(e->Data);
		}

		System::Void Process_Exited(Object^ sender, System::EventArgs^ e) {
			int exitCode = m_process->ExitCode;
			AppendLog(exitCode == 0 ? "-- Compile succeeded. --" : "-- Compile failed. --");
			if (CompileButton->InvokeRequired) {
				CompileButton->Invoke(gcnew MethodInvoker(this, &ScriptCompilerForm::ReenableCompileButton));
			} else {
				ReenableCompileButton();
			}
		}

		void ReenableCompileButton() {
			CompileButton->Enabled = true;
		}
	};
}
