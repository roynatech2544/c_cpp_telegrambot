
#define _CRT_SECURE_NO_WARNINGS
#include <absl/log/log.h>
#include <process.h>
#include <tchar.h>

#include <array>
#include <optional>

#include "../../../src/include/TryParseStr.hpp"
#include "BlockingOperation.h"
#include "TgBotSocketIntf.h"
#include "UIComponents.h"

namespace {

struct SendFileInput {
    ChatId chatid;
    char filePath[MAX_PATH_SIZE];
    HWND dialog;
};
struct SendFileResult {
    bool success;
    char reason[MAX_MSG_SIZE];
};

std::optional<StringLoader::String> OpenFilePicker(HWND hwnd) {
    OPENFILENAME ofn;               // Common dialog box structure
    TCHAR szFile[MAX_PATH_SIZE]{};  // Buffer for file name
    HANDLE hf = nullptr;            // File handle

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the
    // contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = _T("All\0*.*\0Text\0*.TXT\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box
    if (GetOpenFileName(&ofn) == TRUE) return ofn.lpstrFile;
    return std::nullopt;
}

unsigned __stdcall SendFileTask(void* param) {
    const auto* in = (SendFileInput*)param;
    SendFileResult* result = nullptr;

    result = allocMem<SendFileResult>();
    if (result == nullptr) {
        PostMessage(in->dialog, WM_SENDFILE_RESULT, 0, (LPARAM) nullptr);
        free(param);
        return 0;
    }
    sendFileToChat(in->chatid, in->filePath, [result](const GenericAck* data) {
        result->success = data->result == AckType::SUCCESS;
        if (!result->success) {
            strncpy(result->reason, data->error_msg,
                    static_cast<size_t>(MAX_MSG_SIZE) - 1);
        }
    });

    PostMessage(in->dialog, WM_SENDFILE_RESULT, 0, (LPARAM)result);
    free(param);
    return 0;
}

}  // namespace

INT_PTR CALLBACK SendFileToChat(HWND hDlg, UINT message, WPARAM wParam,
                                LPARAM lParam) {
    static HWND hChatId;
    static HWND hFileText;
    static HWND hFileButton;
    static BlockingOperation blk;

    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
        case WM_INITDIALOG:
            hChatId = GetDlgItem(hDlg, IDC_CHATID);
            hFileButton = GetDlgItem(hDlg, IDC_BROWSE);
            hFileText = GetDlgItem(hDlg, IDC_SEL_FILE);
            SetFocus(hChatId);
            return DIALOG_OK;

        case WM_COMMAND:
            if (blk.shouldBlockRequest(hDlg)) {
                return DIALOG_OK;
            }

            switch (LOWORD(wParam)) {
                case IDSEND: {
                    ChatId chatid = 0;
                    std::array<TCHAR, 64 + 1> chatidbuf = {};
                    std::array<char, MAX_PATH_SIZE> pathbuf = {};
                    std::optional<StringLoader::String> errtext;
                    bool fail = false;
                    auto& loader = StringLoader::getInstance();

                    GetDlgItemText(hDlg, IDC_CHATID, chatidbuf.data(),
                                   chatidbuf.size() - 1);
                    GetDlgItemTextA(hDlg, IDC_SEL_FILE, pathbuf.data(),
                                    pathbuf.size());

                    if (Length(chatidbuf.data()) == 0) {
                        errtext = loader.getString(IDS_CHATID_EMPTY);
                    } else if (!try_parse(chatidbuf.data(), &chatid)) {
                        errtext = loader.getString(IDS_CHATID_NOTINT);
                    }
                    if (!errtext.has_value()) {
                        auto* in = allocMem<SendFileInput>();

                        if (in) {
                            blk.start();
                            in->chatid = chatid;
                            in->dialog = hDlg;
                            strncpy(in->filePath, pathbuf.data(),
                                    MAX_PATH_SIZE - 1);
                            in->filePath[MAX_PATH_SIZE - 1] = 0;
                            _beginthreadex(NULL, 0, SendFileTask, in, 0, NULL);
                        } else {
                            errtext = _T("Failed to allocate memory");
                        }
                    }
                    if (errtext) {
                        MessageBox(hDlg, errtext->c_str(),
                                   loader.getString(IDS_FAILED).c_str(),
                                   ERROR_DIALOG);
                    }
                    return DIALOG_OK;
                }
                case IDC_BROWSE:
                    if (const auto f = OpenFilePicker(hDlg); f) {
                        SetDlgItemText(hDlg, IDC_SEL_FILE, f->c_str());
                    }
                    return DIALOG_OK;

                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return DIALOG_OK;
                default:
                    break;
            };
            break;
        case WM_SENDFILE_RESULT: {
            if (lParam == NULL) {
                return DIALOG_OK;
            }
            const auto* res = reinterpret_cast<SendFileResult*>(lParam);
            auto& loader = StringLoader::getInstance();

            if (!res->success) {
                StringLoader::String errtext;
                errtext = loader.getString(IDS_CMD_FAILED_SVR) + kLineBreak;
                errtext += loader.getString(IDS_CMD_FAILED_SVR_RSN);
                errtext += charToWstring(res->reason);
                MessageBox(hDlg, errtext.c_str(),
                           loader.getString(IDS_FAILED).c_str(),
                           ERROR_DIALOG);
            } else {
                MessageBox(hDlg, loader.getString(IDS_SUCCESS_FILESENT).c_str(),
                           loader.getString(IDS_SUCCESS).c_str(),
                           INFO_DIALOG);
            }
            blk.stop();
            free(reinterpret_cast<void*>(lParam));
            return DIALOG_OK;
        }
    }
    return DIALOG_NO;
}
