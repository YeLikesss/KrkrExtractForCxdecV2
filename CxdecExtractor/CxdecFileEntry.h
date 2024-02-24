
#pragma once


class CxdecFileEntry
{
public:
    /// <summary>
    /// 文件夹Hash
    /// </summary>
    unsigned __int8 DirectoryPathHash[8];
    /// <summary>
    /// 文件名Hash
    /// </summary>
    unsigned __int8 FileNameHash[32];
    /// <summary>
    /// 文件Key
    /// </summary>
    __int64 Key;
    /// <summary>
    /// 文件序号
    /// </summary>
    __int64 Ordinal;

    /// <summary>
    /// 获取合法性
    /// </summary>
    __declspec(noinline)
    bool IsVaild()
    {
        return this->Ordinal >= 0;
    }

    /// <summary>
    /// 获取加密模式
    /// </summary>
    __declspec(noinline)
    unsigned __int32 GetEncryptMode()
    {
        return ((this->Ordinal & 0x0000FFFF00000000) >> 32);
    }

    /// <summary>
    /// 获取封包的名字
    /// <para>最多8字节 4个字符 3个Unicode字符 + 0结束符</para>
    /// </summary>
    /// <param name="retValue">字符返回值指针</param>
    __declspec(noinline)
    void GetFakeName(wchar_t* retValue)
    {
        wchar_t* fakeName = retValue;

        *(__int64*)fakeName = 0;      //清空8字节

        unsigned __int32 ordinalLow32 = this->Ordinal & 0x00000000FFFFFFFF;

        int charIndex = 0;
        do
        {
            unsigned long temp = ordinalLow32;
            temp &= 0x00003FFF;
            temp += 0x00005000;

            fakeName[charIndex] = temp & 0x0000FFFF;
            ++charIndex;

            ordinalLow32 >>= 0x0E;
        } while (ordinalLow32 != 0);
    }
};


