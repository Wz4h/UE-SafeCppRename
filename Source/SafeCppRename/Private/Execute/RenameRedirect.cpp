#include "Execute/RenameRedirect.h"

#include "Analysis/BlueprintScanner.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"
#include "AssetRegistry/AssetData.h"


/**
 * 中文注释：
 * 从 Blueprint ParentClassTag 中提取真实 ClassPath
 *
 * 示例：
 * /Script/CoreUObject.Class'/Script/Test.AOldClass'
 * ↓
 * /Script/Test.AOldClass
 */
static FString ExtractClassPath(const FString& Tag)
{
    FString Result = Tag;

    int32 QuoteStart;
    if (Tag.FindChar('\'', QuoteStart))
    {
        int32 QuoteEnd;
        if (Tag.FindLastChar('\'', QuoteEnd))
        {
            Result = Tag.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
        }
    }

    return Result;
}

bool FSafeCppRenameRedirect::GenerateRedirect(
    UClass* OldClass,
    const FString& NewClassName,
    FString& OutReport)
{
    // 中文注释：基本检查
    if (!OldClass)
    {
        OutReport += TEXT("[Redirect] FAILED\n");
        OutReport += TEXT("Reason: OldClass is null\n");
        return false;
    }

    if (NewClassName.IsEmpty())
    {
        OutReport += TEXT("[Redirect] FAILED\n");
        OutReport += TEXT("Reason: NewClassName empty\n");
        return false;
    }

    // =========================
    // 获取旧类路径（UE官方推荐）
    // =========================

    const FString OldPath = OldClass->GetPathName();
    // 示例：/Script/Module.AMyClass

    FString ModuleName;
    FString Left;
    FString Right;

    if (OldPath.Split(TEXT("."), &Left, &Right))
    {
        Left.RemoveFromStart(TEXT("/Script/"));
        ModuleName = Left;
    }

    if (ModuleName.IsEmpty())
    {
        OutReport += TEXT("[Redirect] FAILED\n");
        OutReport += TEXT("Reason: Module name resolve failed\n");
        return false;
    }

    // =========================
    // 自动补齐 C++ 前缀
    // =========================

    const FString Prefix = OldClass->GetPrefixCPP();

    FString NewCppName = NewClassName;
/*
    if (!NewCppName.StartsWith(Prefix))
    {
        NewCppName = Prefix + NewCppName;
    }
*/
    // =========================
    // 生成新类路径
    // =========================

    const FString NewPath =
        FString::Printf(TEXT("/Script/%s.%s"), *ModuleName, *NewCppName);

    // =========================
    // 生成 redirect 行
    // =========================

    const FString RedirectLine =
        FString::Printf(
            TEXT("+ClassRedirects=(OldName=\"%s\",NewName=\"%s\")"),
            *OldPath,
            *NewPath
        );

    // =========================
    // DefaultEngine.ini
    // =========================

    const FString EngineIni =
        FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");

    FString IniText;

    if (FPaths::FileExists(EngineIni))
    {
        FFileHelper::LoadFileToString(IniText, *EngineIni);
    }

    // =========================
    // 防止重复写入 redirect
    // =========================

    if (IniText.Contains(RedirectLine))
    {
        OutReport += TEXT("[Redirect] SKIPPED\n");
        OutReport += TEXT("Reason: Redirect already exists\n");

        OutReport += FString::Printf(
            TEXT("Existing redirect:\n%s\n"),
            *RedirectLine
        );

        return true;
    }

    // =========================
    // 确保 CoreRedirects section
    // =========================

    if (!IniText.Contains(TEXT("[CoreRedirects]")))
    {
        IniText += TEXT("\n[CoreRedirects]\n");
    }

    // =========================
    // 写入 redirect
    // =========================

    IniText += RedirectLine + TEXT("\n");

    if (!FFileHelper::SaveStringToFile(IniText, *EngineIni))
    {
        OutReport += TEXT("[Redirect] FAILED\n");
        OutReport += TEXT("Reason: Write DefaultEngine.ini failed\n");
        return false;
    }

    // =========================
    // 输出报告
    // =========================

    OutReport += TEXT("[Redirect] OK\n");

    OutReport += FString::Printf(
        TEXT("OldClassPath: %s\n"),
        *OldPath
    );

    OutReport += FString::Printf(
        TEXT("NewClassPath: %s\n"),
        *NewPath
    );

    OutReport += TEXT("\nAdded redirect:\n");
    OutReport += RedirectLine;
    OutReport += TEXT("\n");

    OutReport += TEXT("\nNext Steps:\n");
    OutReport += TEXT("1. Restart editor\n");
    OutReport += TEXT("2. Open Blueprints\n");
    OutReport += TEXT("3. Save assets\n");

    return true;
}

