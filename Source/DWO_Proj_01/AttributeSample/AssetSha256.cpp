// Attribute Sample - streaming SHA-256 (FIPS 180-4)

#include "AssetSha256.h"

#include "HAL/FileManager.h"
#include "Serialization/Archive.h"

namespace
{
	constexpr uint32 K[64] =
	{
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	FORCEINLINE uint32 Rotr(uint32 X, uint32 N) { return (X >> N) | (X << (32 - N)); }
}

void FAssetSha256::Reset()
{
	State[0] = 0x6a09e667; State[1] = 0xbb67ae85; State[2] = 0x3c6ef372; State[3] = 0xa54ff53a;
	State[4] = 0x510e527f; State[5] = 0x9b05688c; State[6] = 0x1f83d9ab; State[7] = 0x5be0cd19;
	TotalBytes = 0;
	BufferLength = 0;
}

void FAssetSha256::Transform(const uint8* Block)
{
	uint32 W[64];
	for (int32 i = 0; i < 16; ++i)
	{
		W[i] = (uint32(Block[i * 4]) << 24) | (uint32(Block[i * 4 + 1]) << 16) | (uint32(Block[i * 4 + 2]) << 8) | uint32(Block[i * 4 + 3]);
	}
	for (int32 i = 16; i < 64; ++i)
	{
		const uint32 S0 = Rotr(W[i - 15], 7) ^ Rotr(W[i - 15], 18) ^ (W[i - 15] >> 3);
		const uint32 S1 = Rotr(W[i - 2], 17) ^ Rotr(W[i - 2], 19) ^ (W[i - 2] >> 10);
		W[i] = W[i - 16] + S0 + W[i - 7] + S1;
	}

	uint32 A = State[0], B = State[1], C = State[2], D = State[3], E = State[4], F = State[5], G = State[6], H = State[7];
	for (int32 i = 0; i < 64; ++i)
	{
		const uint32 S1 = Rotr(E, 6) ^ Rotr(E, 11) ^ Rotr(E, 25);
		const uint32 Ch = (E & F) ^ (~E & G);
		const uint32 T1 = H + S1 + Ch + K[i] + W[i];
		const uint32 S0 = Rotr(A, 2) ^ Rotr(A, 13) ^ Rotr(A, 22);
		const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
		const uint32 T2 = S0 + Maj;
		H = G; G = F; F = E; E = D + T1; D = C; C = B; B = A; A = T1 + T2;
	}
	State[0] += A; State[1] += B; State[2] += C; State[3] += D;
	State[4] += E; State[5] += F; State[6] += G; State[7] += H;
}

void FAssetSha256::Update(const uint8* Data, int64 Length)
{
	TotalBytes += uint64(Length);
	while (Length > 0)
	{
		const uint32 Take = uint32(FMath::Min<int64>(64 - BufferLength, Length));
		FMemory::Memcpy(Buffer + BufferLength, Data, Take);
		BufferLength += Take;
		Data += Take;
		Length -= Take;
		if (BufferLength == 64)
		{
			Transform(Buffer);
			BufferLength = 0;
		}
	}
}

FString FAssetSha256::FinalizeHex()
{
	const uint64 BitLength = TotalBytes * 8;
	const uint8 Pad = 0x80;
	const uint8 Zero = 0;

	// Update() also counts bytes, so pad without touching TotalBytes
	const uint64 SavedTotal = TotalBytes;
	Update(&Pad, 1);
	while (BufferLength != 56)
	{
		Update(&Zero, 1);
	}
	uint8 LengthBytes[8];
	for (int32 i = 0; i < 8; ++i)
	{
		LengthBytes[i] = uint8(BitLength >> (56 - i * 8));
	}
	Update(LengthBytes, 8);
	TotalBytes = SavedTotal;

	FString Hex;
	Hex.Reserve(64);
	for (uint32 Word : State)
	{
		Hex += FString::Printf(TEXT("%08x"), Word);
	}
	return Hex;
}

FString FAssetSha256::HashBytes(const TArray<uint8>& Bytes)
{
	FAssetSha256 Sha;
	Sha.Update(Bytes.GetData(), Bytes.Num());
	return Sha.FinalizeHex();
}

bool FAssetSha256::HashFile(const FString& FilePath, FString& OutHex, int64* OutSize)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Reader)
	{
		return false;
	}

	FAssetSha256 Sha;
	TArray<uint8> Chunk;
	Chunk.SetNumUninitialized(1024 * 1024);
	const int64 Size = Reader->TotalSize();
	int64 Remaining = Size;
	while (Remaining > 0)
	{
		const int64 Take = FMath::Min<int64>(Chunk.Num(), Remaining);
		Reader->Serialize(Chunk.GetData(), Take);
		if (Reader->IsError())
		{
			return false;
		}
		Sha.Update(Chunk.GetData(), Take);
		Remaining -= Take;
	}

	OutHex = Sha.FinalizeHex();
	if (OutSize)
	{
		*OutSize = Size;
	}
	return true;
}
