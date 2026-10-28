param([ValidateSet('keygen','decrypt','encrypt')][string]$Mode, [string]$InputFile, [string]$OutputFile, [string]$KeyFile)
$ErrorActionPreference = 'Stop'
$rsa = [System.Security.Cryptography.RSA]::Create(3072)
try {
    if ($Mode -eq 'keygen') {
        [IO.File]::WriteAllText($KeyFile, $rsa.ExportPkcs8PrivateKeyPem())
        [IO.File]::WriteAllText($OutputFile, $rsa.ExportSubjectPublicKeyInfoPem())
        exit 0
    }
    $rsa.ImportFromPem([IO.File]::ReadAllText($KeyFile))
    if ($Mode -eq 'decrypt') {
        $envelope = Get-Content $InputFile -Raw | ConvertFrom-Json
        $key = $rsa.Decrypt([Convert]::FromBase64String($envelope.key), [Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
        $ciphertext = [Convert]::FromBase64String($envelope.ciphertext)
        $plaintext = [byte[]]::new($ciphertext.Length)
        $aes = [Security.Cryptography.AesGcm]::new($key,16)
        try { $aes.Decrypt([Convert]::FromBase64String($envelope.nonce),$ciphertext,[Convert]::FromBase64String($envelope.tag),$plaintext) } finally { $aes.Dispose() }
        [IO.File]::WriteAllBytes($OutputFile,$plaintext)
    } else {
        $key = [Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
        $nonce = [Security.Cryptography.RandomNumberGenerator]::GetBytes(12)
        $plaintext = [IO.File]::ReadAllBytes($InputFile)
        $ciphertext = [byte[]]::new($plaintext.Length)
        $tag = [byte[]]::new(16)
        $aes = [Security.Cryptography.AesGcm]::new($key,16)
        try { $aes.Encrypt($nonce,$plaintext,$ciphertext,$tag) } finally { $aes.Dispose() }
        $envelope = @{
            key = [Convert]::ToBase64String($rsa.Encrypt($key,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256))
            nonce = [Convert]::ToBase64String($nonce)
            ciphertext = [Convert]::ToBase64String($ciphertext)
            tag = [Convert]::ToBase64String($tag)
        }
        $envelope | ConvertTo-Json -Compress | Set-Content $OutputFile
    }
} finally { $rsa.Dispose() }
