// zip.cpp
/*
 *  Copyright (c) 2012 Leigh Johnston.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of Leigh Johnston nor the names of any
 *       other contributors to this software may be used to endorse or
 *       promote products derived from this software without specific prior
 *       written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "neolib.hpp"
#include <fstream>
#include <string>
#include <zlib/zlib.h>
#include "string_utils.hpp"
#include "crc.hpp"
#include "zip.hpp"

#pragma pack(1)

namespace neolib
{
	namespace
	{
		template <typename T>
		inline const char* byte_cast(const T* p)
		{
			return reinterpret_cast<const char*>(p);
		}
	}

	template <typename CharT>
	struct basic_zip<CharT>::local_header
	{
		enum { Signature = 0x04034b50 };
		dword iSignature;
		word iVersion;
		word iFlag;
		word iCompression;
		word iTime;
		word iDate;
		dword iCrc32;
		dword iCompressedSize;
		dword iUncompressedSize;
		word iFilenameLength;
		word iExtraLength;
	};

	template <typename CharT>
	struct basic_zip<CharT>::dir_header
	{
		enum { Signature = 0x06054b50 };
		dword iSignature;
		word iDisk;
		word iStartDisk;
		word iDirEntries;
		word iTotalDirEntries;
		dword iDirSize;
		dword iDirOffset;
		word iCommentLength;
	};

	template <typename CharT>
	struct basic_zip<CharT>::dir_file_header
	{
		enum { Signature = 0x02014b50 };
		dword iSignature;
		word iVersionMade;
		word iVersionNeeded;
		word iFlag;
		word iCompression;
		word iTime;
		word iDate;
		dword iCrc32;
		dword iCompressedSize;
		dword iUncompressedSize;
		word iFilenameLength;
		word iExtraLength;
		word iCommentLength;
		word iDiskStart;
		word iIntAttr;
		dword iExtAttr;
		dword iHeaderOffset;
	};

	#pragma pack()

	template <typename CharT>
	basic_zip<CharT>::basic_zip(const buffer_t& aZipFile, const typename path_type& aTargetDirectory)  : iZipFile(aZipFile), iTargetDirectory(aTargetDirectory), iError(false)
	{
		parse();
	}

	template <typename CharT>
	basic_zip<CharT>::basic_zip(const buffer_ptr_t& aZipFilePtr, const typename path_type& aTargetDirectory) : iZipFile(*aZipFilePtr), iZipFilePtr(aZipFilePtr), iTargetDirectory(aTargetDirectory), iError(false)
	{
		parse();
	}

	template <typename CharT>
	bool basic_zip<CharT>::extract(size_t aIndex)
	{
		if (iError)
			return false;
		if (aIndex > iFiles.size())
			return false;
		const local_header* lh = reinterpret_cast<const local_header*>(&iZipFile.front() + iDirEntries[aIndex]->iHeaderOffset);
		if (byte_cast(lh) < byte_cast(&iZipFile.front()) ||
			byte_cast(lh) > byte_cast(&iZipFile.back()) - sizeof(local_header) + 1)
		{
			iError = true;
			return false;
		}
		if (lh->iSignature != local_header::Signature)
		{
			iError = true;
			return false;
		}
		const uint8_t* compressedData = reinterpret_cast<const uint8_t*>(lh+1) + lh->iFilenameLength + lh->iExtraLength;
		if (byte_cast(compressedData) < byte_cast(&iZipFile.front()) ||
			byte_cast(compressedData) + lh->iCompressedSize -1 > byte_cast(&iZipFile.back()))
		{
			iError = true;
			return false;
		}
		if (lh->iCompression == Z_NO_COMPRESSION)
		{
			if (crc32(reinterpret_cast<const uint8_t*>(compressedData), lh->iCompressedSize) != lh->iCrc32)
			{
				iError = true;
				return false;
			}
			std::ofstream out(file_path(aIndex).c_str(), std::ios::out|std::ios::binary);
			if (!out)
			{
				iError = true;
				return false;
			}
			out.write(reinterpret_cast<const char*>(compressedData), lh->iCompressedSize);
			return true;
		}
		else if (lh->iCompression != Z_DEFLATED)
		{
			iError = true;
			return false;
		}
		z_stream stream;
		stream.next_in = static_cast<Bytef*>(const_cast<uint8_t*>(compressedData));
		stream.avail_in = static_cast<uInt>(lh->iCompressedSize);
		std::vector<uint8_t> decompressedData;
		decompressedData.resize(lh->iUncompressedSize);
		stream.next_out = static_cast<Bytef*>(&decompressedData[0]);
		stream.avail_out = decompressedData.size();
		stream.zalloc = static_cast<alloc_func>(0);
		stream.zfree = static_cast<free_func>(0);
		int result = inflateInit2(&stream, -MAX_WBITS);
		if (result == Z_OK)
		{
			result = inflate(&stream, Z_FINISH);
			inflateEnd(&stream);
			if (result == Z_STREAM_END)
				result = Z_OK;
			inflateEnd(&stream);
		}
		if (result != Z_OK)
		{
			iError = true;
			return false;
		}
		if (crc32(reinterpret_cast<const uint8_t*>(&decompressedData[0]), decompressedData.size()) != lh->iCrc32)
		{
			iError = true;
			return false;
		}
		std::ofstream out(file_path(aIndex).c_str(), std::ios::out|std::ios::binary);
		if (!out)
		{
			iError = true; 
			return false;
		}
		out.write(reinterpret_cast<const char*>(&decompressedData[0]), decompressedData.size());
		return true;
	}

	namespace
	{
		template <typename CharT>
		struct path_dep
		{
			static const CharT* sSeparator;
			static typename basic_zip<CharT>::path_type narrow_to_wide(const std::string& aNarrow) { return aNarrow; 	}
		};
		template <typename CharT>
		const CharT* path_dep<CharT>::sSeparator = "/";
		template <>
		const wchar_t* path_dep<wchar_t>::sSeparator = L"/";
		template <>
		typename basic_zip<wchar_t>::path_type path_dep<wchar_t>::narrow_to_wide(const std::string& aNarrow) { return neolib::narrow_to_wide(aNarrow); }
	}

	template <typename CharT>
	typename basic_zip<CharT>::path_type basic_zip<CharT>::file_path(size_t aIndex) const
	{
		return iTargetDirectory + path_dep<CharT>::sSeparator + iFiles[aIndex];
	}

	template <typename CharT>
	bool basic_zip<CharT>::parse()
	{
		if (iZipFile.size() < sizeof(dir_header))
		{
			iError = true;
			return false;
		}
		const dir_header* dh = reinterpret_cast<const dir_header*>(&iZipFile.back() - sizeof(dir_header) + 1);
		if (dh->iSignature != dir_header::Signature)
		{
			iError = true;
			return false;
		}
		const dir_file_header* fh = reinterpret_cast<const dir_file_header*>(&iZipFile.back() - (dh->iDirSize + sizeof(dir_header)) + 1);
		for (size_t i = 0; i < dh->iDirEntries; ++i)
		{
			if (byte_cast(fh) < byte_cast(&iZipFile.front()) ||
				byte_cast(fh) + sizeof(fh->iSignature) >= byte_cast(dh))
			{
				iError = false;
				return false;
			}
			if (fh->iSignature != dir_file_header::Signature)
			{
				iError = false;
				return false;
			}
			else
			{
				if (byte_cast(reinterpret_cast<const char*>(fh) + sizeof(*fh) + fh->iFilenameLength + fh->iExtraLength + fh->iCommentLength) > 
					byte_cast(dh))
				{
					iError = false;
					return false;
				}
				iDirEntries.push_back(fh);
				std::string filename(reinterpret_cast<const char*>(fh + 1), fh->iFilenameLength);
				iFiles.push_back(path_dep<CharT>::narrow_to_wide(filename));
				fh = reinterpret_cast<const dir_file_header*>(reinterpret_cast<const char*>(fh) + sizeof(*fh) + fh->iFilenameLength + fh->iExtraLength + fh->iCommentLength);
			}
		}
		return true;
	}

	template class basic_zip<char>;
	template class basic_zip<wchar_t>;
}