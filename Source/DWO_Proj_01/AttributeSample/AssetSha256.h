// Attribute Sample - streaming SHA-256 (FIPS 180-4)
//
// FPlatformMisc::GetSHA256Signature has no Windows implementation in UE 5.7 (the generic version asserts),
// so the asset sync computes hashes with this small, dependency-free implementation. Output is lower-case
// hex, identical to Python's hashlib.sha256().hexdigest() used by the asset server.

#pragma once

#include "CoreMinimal.h"

class DWO_PROJ_01_API FAssetSha256
{
public:
	FAssetSha256() { Reset(); }

	void Reset();
	void Update(const uint8* Data, int64 Length);
	/** Finishes the hash and returns 64 lower-case hex characters. Call Reset() before reusing. */
	FString FinalizeHex();

	static FString HashBytes(const TArray<uint8>& Bytes);

	/** Streams the file in 1 MB chunks (never loads it whole). Safe to call from a worker thread. */
	static bool HashFile(const FString& FilePath, FString& OutHex, int64* OutSize = nullptr);

private:
	void Transform(const uint8* Block);

	uint32 State[8];
	uint64 TotalBytes = 0;
	uint8 Buffer[64];
	uint32 BufferLength = 0;
};
