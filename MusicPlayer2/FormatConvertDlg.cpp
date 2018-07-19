// FormatConvertDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "MusicPlayer2.h"
#include "FormatConvertDlg.h"
#include "afxdialogex.h"

CBASSEncodeLibrary CFormatConvertDlg::m_bass_encode_lib;
CBASSWmaLibrary CFormatConvertDlg::m_bass_wma_lib;

// CFormatConvertDlg 对话框

IMPLEMENT_DYNAMIC(CFormatConvertDlg, CDialog)

CFormatConvertDlg::CFormatConvertDlg(const vector<int>& items_selected, CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_FORMAT_CONVERT_DIALOG, pParent)
{
	//获取文件列表
	for (const auto& index : items_selected)
	{
		const SongInfo& song{ theApp.m_player.GetPlayList()[index] };
		wstring file_path{ theApp.m_player.GetCurrentDir() + song.file_name };
		if(!CCommon::IsItemInVector(m_file_list, file_path))
			m_file_list.push_back(theApp.m_player.GetCurrentDir() + song.file_name);
	}
}

CFormatConvertDlg::~CFormatConvertDlg()
{
	m_bass_encode_lib.UnInit();
	m_bass_wma_lib.UnInit();
}

void CFormatConvertDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SONG_LIST1, m_file_list_ctrl);
	DDX_Control(pDX, IDC_OUT_FORMAT_COMBO, m_encode_format_combo);
}


BEGIN_MESSAGE_MAP(CFormatConvertDlg, CDialog)
	ON_CBN_SELCHANGE(IDC_OUT_FORMAT_COMBO, &CFormatConvertDlg::OnCbnSelchangeOutFormatCombo)
	ON_BN_CLICKED(IDC_START_CONVERT_BUTTON, &CFormatConvertDlg::OnBnClickedStartConvertButton)
	ON_BN_CLICKED(IDC_BROWSE_BUTTON, &CFormatConvertDlg::OnBnClickedBrowseButton)
	ON_MESSAGE(WM_CONVERT_PROGRESS, &CFormatConvertDlg::OnConvertProgress)
	ON_MESSAGE(WM_CONVERT_COMPLETE, &CFormatConvertDlg::OnConvertComplete)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_ENCODER_CONFIG_BUTTON, &CFormatConvertDlg::OnBnClickedEncoderConfigButton)
END_MESSAGE_MAP()


// CFormatConvertDlg 消息处理程序


BOOL CFormatConvertDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化

	//初始化文件列表
	CRect rect;
	m_file_list_ctrl.GetClientRect(rect);
	int width0, width1, width2;
	width0 = rect.Width() / 10;
	width2 = rect.Width() * 2 / 10;
	width1 = rect.Width() - width0 - width2 - DPI(21);
	//插入列
	m_file_list_ctrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
	m_file_list_ctrl.InsertColumn(0, _T("序号"), LVCFMT_LEFT, width0);		//插入第1列
	m_file_list_ctrl.InsertColumn(1, _T("文件名"), LVCFMT_LEFT, width1);		//插入第2列
	m_file_list_ctrl.InsertColumn(2, _T("转换状态"), LVCFMT_LEFT, width2);		//插入第3列
	//插入项目
	for (size_t i{}; i<m_file_list.size(); i++)
	{
		CString tmp;
		tmp.Format(_T("%d"), i + 1);
		m_file_list_ctrl.InsertItem(i, tmp);
		CFilePathHelper file_path(m_file_list[i]);
		m_file_list_ctrl.SetItemText(i, 1, file_path.GetFileName().c_str());
	}
	//设置主题颜色
	m_file_list_ctrl.SetColor(theApp.m_app_setting_data.theme_color);

	//初始化转换格式的下拉列表
	m_encode_format_combo.AddString(_T("WAV"));
	m_encode_format_combo.AddString(_T("MP3 (lame编码器)"));
	m_encode_format_combo.AddString(_T("WMA"));
	m_encode_format_combo.AddString(_T("OGG"));
	m_encode_format_combo.SetCurSel(static_cast<int>(m_encode_format));

	m_encoder_successed = InitEncoder();


	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void CFormatConvertDlg::EnableControls(bool enable)
{
	GetDlgItem(IDC_OUT_FORMAT_COMBO)->EnableWindow(enable);
	GetDlgItem(IDC_BROWSE_BUTTON)->EnableWindow(enable);
	GetDlgItem(IDC_START_CONVERT_BUTTON)->EnableWindow(enable);
	GetDlgItem(IDC_ENCODER_CONFIG_BUTTON)->EnableWindow(enable);
}

bool CFormatConvertDlg::InitEncoder()
{
	//初始化bass encode库
	wstring encode_dll_path;
	wstring wma_dll_path;
	wstring plugin_dir;
#ifdef _DEBUG
	m_encode_dir = L".\\Encoder\\";
	plugin_dir = L".\\Plugins\\";
#else
	m_encode_dir = theApp.m_module_dir + L"Encoder\\";
	plugin_dir = theApp.m_module_dir + L"Plugins\\";
#endif // _DEBUG
	encode_dll_path = m_encode_dir + L"bassenc.dll";
	m_bass_encode_lib.Init(encode_dll_path);

	wma_dll_path = plugin_dir + L"basswma.dll";
	m_bass_wma_lib.Init(wma_dll_path);

	return m_bass_encode_lib.IsSuccessed();
}

bool CFormatConvertDlg::EncodeSingleFile(CFormatConvertDlg* pthis, int file_index, wstring& out_file_path, const SongInfo& song_info)
{
	const wstring& file_path{ pthis->m_file_list[file_index] };
	const int BUFF_SIZE{ 20000 };
	char buff[BUFF_SIZE];
	//创建解码通道
	HSTREAM hStream = BASS_StreamCreateFile(FALSE, file_path.c_str(), 0, 0, BASS_STREAM_DECODE/* | BASS_SAMPLE_FLOAT*/);
	if (hStream == 0)
	{
		::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, -1);
		return false;
	}
	bool is_midi = (CAudioCommon::GetAudioType(file_path) == AU_MIDI);
	if (theApp.m_player.m_bass_midi_lib.IsSuccessed() && is_midi && theApp.m_player.GetSoundFont().font != 0)
		theApp.m_player.m_bass_midi_lib.BASS_MIDI_StreamSetFonts(hStream, &theApp.m_player.GetSoundFont(), 1);
	//获取流的长度
	QWORD length = BASS_ChannelGetLength(hStream, 0);
	if (length == 0) length = 1;	//防止length作为除数时为0
	//设置输出文件路径
	CFilePathHelper c_file_path(file_path);
	switch (pthis->m_encode_format)
	{
	case EncodeFormat::WAV:
		c_file_path.ReplaceFileExtension(L"wav");
		break;
	case EncodeFormat::MP3:
		c_file_path.ReplaceFileExtension(L"mp3");
		break;
	case EncodeFormat::WMA:
		c_file_path.ReplaceFileExtension(L"wma");
		break;
	case EncodeFormat::OGG:
		c_file_path.ReplaceFileExtension(L"ogg");
		break;
	}
	//wstring out_file_path;
	out_file_path = pthis->m_out_dir + c_file_path.GetFileName();

	HENCODE hEncode;
	if (pthis->m_encode_format != EncodeFormat::WMA)
	{
		//设置解码器命令行参数
		wstring cmdline;
		switch (pthis->m_encode_format)
		{
		case EncodeFormat::MP3:
		{
			//获取源文件的专辑封面
			wstring album_cover_path;
			SongInfo song_info_tmp;
			CAudioTag audio_tag(hStream, file_path, song_info_tmp);
			int cover_type;
			album_cover_path = audio_tag.GetAlbumCover(cover_type, ALBUM_COVER_NAME_ENCODE);
			CImage image;
			image.Load(album_cover_path.c_str());
			if (image.IsNull())		//如果没有内嵌的专辑封面，则获取外部封面
				album_cover_path = CPlayer::GetRelatedAlbumCover(file_path, song_info);
			else
				image.Destroy();
			int album_cover_size = CCommon::GetFileSize(album_cover_path);
			bool write_album_cover{ !album_cover_path.empty() && album_cover_size > 0 && album_cover_size < 128 * 1024 };	//是否要写入专辑封面（专辑封面文件必须小于128KB）

			//设置lame命令行参数
			cmdline = pthis->m_encode_dir;
			cmdline += L"lame.exe ";
			cmdline += pthis->m_mp3_encode_para.cmd_para;
			CString str;
			str.Format(_T(" --tt \"%s\" --ta \"%s\" --tl \"%s\" --ty \"%s\" --tc \"%s\" --tn \"%d\" --tg \"%s\""),
				song_info.title.c_str(), song_info.artist.c_str(), song_info.album.c_str(), song_info.year.c_str(), song_info.comment.c_str(),
				song_info.track, song_info.genre.c_str());
			cmdline += str;
			if (write_album_cover)
			{
				str.Format(_T(" --ti \"%s\""), album_cover_path.c_str());
				cmdline += str;
			}
			cmdline += L" --add-id3v2";
			cmdline += L" - \"";
			cmdline += out_file_path;
			cmdline.push_back(L'\"');
		}
			break;
		case EncodeFormat::OGG:
		{
			cmdline = pthis->m_encode_dir;
			CString str;
			str.Format(_T("oggenc -q %d -t \"%s\" -a \"%s\" -l \"%s\" -N \"%d\" -o \""),
				pthis->m_ogg_encode_quality, song_info.title.c_str(), song_info.artist.c_str(), song_info.album.c_str(), song_info.track);
			cmdline += str;
			cmdline += out_file_path;
			cmdline += L"\" -";
		}
			break;
		default:
			cmdline = out_file_path.c_str();
		}
		//开始解码
		hEncode = m_bass_encode_lib.BASS_Encode_StartW(hStream, cmdline.c_str(), BASS_ENCODE_AUTOFREE | (pthis->m_encode_format == EncodeFormat::WAV ? BASS_ENCODE_PCM : 0), NULL, 0);
		if (hEncode == 0)
		{
			BASS_StreamFree(hStream);
			::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, -2);
			return false;
		}
		while (BASS_ChannelIsActive(hStream) != 0)
		{
			if (theApp.m_format_convert_dialog_exit)
			{
				m_bass_encode_lib.BASS_Encode_Stop(hEncode);
				BASS_StreamFree(hStream);
				return false;
			}

			BASS_ChannelGetData(hStream, buff, BUFF_SIZE);
			if (m_bass_encode_lib.BASS_Encode_IsActive(hStream) == 0)
			{
				m_bass_encode_lib.BASS_Encode_Stop(hEncode);
				BASS_StreamFree(hStream);
			}

			//获取转换百分比
			QWORD position = BASS_ChannelGetPosition(hStream, 0);
			static int last_percent{ -1 };
			int percent = static_cast<int>(position * 100 / length);
			if (percent < 0 || percent>100)
			{
				::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, -3);
				BASS_StreamFree(hStream);
				return false;
			}
			if (last_percent != percent)
				::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, percent);
			last_percent = percent;
		}
	}
	else		//输出为WMA时使用专用的编码器
	{
		if(!m_bass_wma_lib.IsSuccessed())
		{
			BASS_StreamFree(hStream);
			return false;
		}
		//开始解码
		hEncode = m_bass_wma_lib.BASS_WMA_EncodeOpenFileW(hStream, NULL, BASS_WMA_ENCODE_STANDARD | BASS_WMA_ENCODE_SOURCE, pthis->m_wma_encode_bitrate*1000, out_file_path.c_str());
		//int error = BASS_ErrorGetCode();
		if (hEncode == 0)
		{
			BASS_StreamFree(hStream);
			::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, -2);
			return false;
		}
		//写入WMA标签
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"Title", song_info.title.c_str());
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"Author", song_info.artist.c_str());
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"WM/AlbumTitle", song_info.album.c_str());
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"WM/Year", song_info.year.c_str());
		CString track;
		track.Format(_T("%d"), song_info.track);
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"WM/TrackNumber", track.GetString());
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"WM/Genre", song_info.genre.c_str());
		m_bass_wma_lib.BASS_WMA_EncodeSetTagW(hEncode, L"Description", song_info.comment.c_str());

		while (BASS_ChannelIsActive(hStream) != BASS_ACTIVE_STOPPED)
		{
			if (theApp.m_format_convert_dialog_exit)
			{
				m_bass_encode_lib.BASS_Encode_Stop(hEncode);
				BASS_StreamFree(hStream);
				return false;
			}
			//if (m_bass_wma_lib.BASS_WMA_EncodeWrite(hEncode, buff, BASS_ChannelGetData(hStream, buff, BUFF_SIZE)) == FALSE)
			//	break;
			BASS_ChannelGetData(hStream, buff, BUFF_SIZE);
			//获取转换百分比
			QWORD position = BASS_ChannelGetPosition(hStream, 0);
			static int last_percent{ -1 };
			int percent = static_cast<int>(position * 100 / length);
			if (percent < 0 || percent>100)
			{
				::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, -3);
				BASS_StreamFree(hStream);
				return false;
			}
			if (last_percent != percent)
				::PostMessage(pthis->GetSafeHwnd(), WM_CONVERT_PROGRESS, file_index, percent);
			last_percent = percent;
		}
	}

	BASS_StreamFree(hStream);
	return true;
}


UINT CFormatConvertDlg::ThreadFunc(LPVOID lpParam)
{
	CFormatConvertDlg* pThis{ (CFormatConvertDlg*)lpParam };
	for (size_t i{}; i < pThis->m_file_list.size(); i++)
	{
		if (theApp.m_format_convert_dialog_exit)
			return 0;
		wstring out_file_path;
		SongInfo song_info = theApp.m_song_data[pThis->m_file_list[i]];
		if (CAudioCommon::GetAudioType(pThis->m_file_list[i]) == AudioType::AU_MIDI)
		{
			//如果被转换的文件是 MIDI 音乐，则把SF2音色库名称保存到注释信息
			CString comment;
			comment.Format(_T("MIDI SF2: %s"), theApp.m_player.GetSoundFontName().c_str());
			song_info.comment = comment;
		}
		//编码文件
		if(EncodeSingleFile(pThis, i, out_file_path, song_info))
			::PostMessage(pThis->GetSafeHwnd(), WM_CONVERT_PROGRESS, i, 101);
	}
	::PostMessage(pThis->GetSafeHwnd(), WM_CONVERT_COMPLETE, 0, 0);
	return 0;
}


void CFormatConvertDlg::OnCbnSelchangeOutFormatCombo()
{
	// TODO: 在此添加控件通知处理程序代码
	m_encode_format = static_cast<EncodeFormat>(m_encode_format_combo.GetCurSel());
	GetDlgItem(IDC_ENCODER_CONFIG_BUTTON)->EnableWindow(m_encode_format != EncodeFormat::WAV);
}


void CFormatConvertDlg::OnBnClickedStartConvertButton()
{
	// TODO: 在此添加控件通知处理程序代码
	if (!m_encoder_successed)
	{
		CString info;
		info.Format(_T("BASS 编码器加载失败，文件 %s 可能已经损坏！"), (m_encode_dir + L"bassenc.dll").c_str());
		MessageBox(info, NULL, MB_ICONERROR | MB_OK);
		return;
	}

	if (m_out_dir.empty())
	{
		MessageBox(_T("请设置输出目录！"), NULL, MB_ICONWARNING | MB_OK);
		return;
	}
	else if (!CCommon::FolderExist(m_out_dir))
	{
		CString info;
		info.Format(_T("输出目录 \"%s\" 不存在！"), m_out_dir.c_str());
		MessageBox(info, NULL, MB_ICONWARNING | MB_OK);
	}

	//先清除“状态”一列的内容
	for (size_t i{}; i < m_file_list.size(); i++)
	{
		m_file_list_ctrl.SetItemText(i, 2, _T(""));
	}

	EnableControls(false);
	theApp.m_format_convert_dialog_exit = false;
	//创建格式转换的工作线程
	m_pThread = AfxBeginThread(ThreadFunc, this);

}


void CFormatConvertDlg::OnBnClickedBrowseButton()
{
	// TODO: 在此添加控件通知处理程序代码
#ifdef COMPILE_IN_WIN_XP
	CFolderBrowserDlg folderPickerDlg(this->GetSafeHwnd());
	folderPickerDlg.SetInfo(_T("请选择输出文件夹。"));
#else
	CFolderPickerDialog folderPickerDlg(m_out_dir.c_str());
	folderPickerDlg.m_ofn.lpstrTitle = _T("选择输出文件夹");		//设置对话框标题
#endif // COMPILE_IN_WIN_XP
	if (folderPickerDlg.DoModal() == IDOK)
	{
		m_out_dir = folderPickerDlg.GetPathName();
		if (m_out_dir.back() != L'\\') m_out_dir.push_back(L'\\');	//确保路径末尾有反斜杠
		SetDlgItemText(IDC_OUT_DIR_EDIT, m_out_dir.c_str());
	}

}


afx_msg LRESULT CFormatConvertDlg::OnConvertProgress(WPARAM wParam, LPARAM lParam)
{
	CString percent_str;
	int percent = (int)lParam;
	if(percent < 0)
		percent_str.Format(_T("出错 (%d)"), percent);
	else if(percent == 101)
		percent_str = _T("完成");
	else
		percent_str.Format(_T("%d%%"), (int)lParam);
	m_file_list_ctrl.SetItemText(wParam, 2, percent_str);

	return 0;
}


afx_msg LRESULT CFormatConvertDlg::OnConvertComplete(WPARAM wParam, LPARAM lParam)
{
	EnableControls(true);
	return 0;
}


void CFormatConvertDlg::OnCancel()
{
	// TODO: 在此添加专用代码和/或调用基类
	//theApp.m_format_convert_dialog_exit = true;
	//if (m_pThread != nullptr)
	//	WaitForSingleObject(m_pThread->m_hThread, 2000);	//等待线程退出

	CDialog::OnCancel();
}


void CFormatConvertDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类
	//theApp.m_format_convert_dialog_exit = true;
	//if (m_pThread != nullptr)
	//	WaitForSingleObject(m_pThread->m_hThread, 2000);	//等待线程退出

	CDialog::OnOK();
}


void CFormatConvertDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	theApp.m_format_convert_dialog_exit = true;
	if (m_pThread != nullptr)
		WaitForSingleObject(m_pThread->m_hThread, 2000);	//等待线程退出

	CDialog::OnClose();
}


void CFormatConvertDlg::OnBnClickedEncoderConfigButton()
{
	// TODO: 在此添加控件通知处理程序代码
	switch (m_encode_format)
	{
		case EncodeFormat::WAV:
			break;
		case EncodeFormat::MP3:
		{
			CMP3EncodeCfgDlg dlg;
			dlg.m_encode_para = m_mp3_encode_para;
			if(dlg.DoModal() == IDOK)
				m_mp3_encode_para = dlg.m_encode_para;
		}
			break;
		case EncodeFormat::WMA:
		{
			CWmaEncodeCfgDlg dlg;
			dlg.m_bitrate = m_wma_encode_bitrate;
			if (dlg.DoModal() == IDOK)
				m_wma_encode_bitrate = dlg.m_bitrate;
		}
			break;
		case EncodeFormat::OGG:
		{
			COggEncodeCfgDlg dlg;
			dlg.m_encode_quality = m_ogg_encode_quality;
			if (dlg.DoModal() == IDOK)
				m_ogg_encode_quality = dlg.m_encode_quality;
		}
			break;
		default:
			break;
	}
}
