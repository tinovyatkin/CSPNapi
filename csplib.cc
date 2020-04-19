#include <napi.h>
#include <stdio.h>
#include <CSP_WinDef.h>
#include <CSP_WinCrypt.h>
// #include <WinCryptEx.h>
// #define szOID_CP_GOST_R3411_12_256 "1.2.643.7.1.1.2.2"
#define TYPE_DER (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

typedef struct StructResult {
    int status;
    DWORD errorCode;
    char *errorMessage;
} StructResult;

StructResult RetSuccess() {
    StructResult result = {0, 0, (char*)""};
    return result;
}

StructResult RetError(char *errorMessage) {
    DWORD errorCode = GetLastError();
    if(!errorCode) 
        errorCode = 1;
    StructResult result = {
        errorCode, 
        errorCode, 
        errorMessage
    };
    printf("Error number     : 0x%x\n", result.errorCode);
    printf("Error description: %s\n", result.errorMessage);
    return result;
}

StructResult doSign(const BYTE *mem_tbs, const DWORD mem_len, BYTE **signedMsg, DWORD *signedLen)
{
    LPWSTR certificateSubjectKey = L"cbd";
    LPSTR szOID_CP_GOST_R3411_12_256 = "1.2.643.7.1.1.2.2";
    PCCERT_CONTEXT pCertContext = NULL; // Контекст сертификата
    HCERTSTORE hStoreHandle = 0;        // Дескриптор хранилища сертификатов
    CRYPT_SIGN_MESSAGE_PARA SigParams;

    // Открытие системного хранилища сертификатов.
    hStoreHandle = CertOpenSystemStore(0, "MY");

    if (!hStoreHandle)
    {
        return RetError("Ошибка открытия хранилища сертификатов");
    }

    pCertContext = CertFindCertificateInStore(
        hStoreHandle,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_SUBJECT_STR,
        certificateSubjectKey,
        NULL);

    if (!pCertContext)
    {
        return RetError("Ошибка поиска сертификата");
    }

    ZeroMemory(&SigParams, sizeof(SigParams));
    SigParams.cbSize = sizeof(CRYPT_SIGN_MESSAGE_PARA);
    SigParams.dwMsgEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    SigParams.pSigningCert = pCertContext;
    SigParams.HashAlgorithm.pszObjId = szOID_CP_GOST_R3411_12_256;
    SigParams.HashAlgorithm.Parameters.cbData = 0;
    SigParams.HashAlgorithm.Parameters.pbData = NULL;
    SigParams.cMsgCert = 0;
    SigParams.rgpMsgCert = NULL;
    SigParams.cAuthAttr = 0;
    SigParams.dwInnerContentType = 0;
    SigParams.cMsgCrl = 0;
    SigParams.cUnauthAttr = 0;
    SigParams.dwFlags = 0;
    // SigParams.dwFlags = CRYPT_MESSAGE_SILENT_KEYSET_FLAG;
    SigParams.pvHashAuxInfo = NULL;
    SigParams.rgAuthAttr = NULL;
    SigParams.pvHashAuxInfo = NULL; /* not used*/

    // Определение длины подписанного сообщения
    const BYTE *pbMessageBuffers[] = {mem_tbs};
    DWORD cbMessageSizes[] = {mem_len};
    if (!CryptSignMessage(&SigParams, FALSE, 1, pbMessageBuffers, cbMessageSizes, NULL, signedLen))
    {
        return RetError("Ошибка определения длины подписанного сообщения");
    }

    // Подпись сообщения
    *signedMsg = (BYTE *)malloc(sizeof(BYTE) * *signedLen);
    if (!CryptSignMessage(&SigParams, FALSE, 1, pbMessageBuffers, cbMessageSizes, *signedMsg, signedLen))
    {
        free(*signedMsg);
        return RetError("Ошибка подписанния сообщения");
    }

    return RetSuccess();
}

StructResult doVerify(const BYTE *mem_tbs, const DWORD mem_len, BYTE **Msg, DWORD *Len)
{
    CRYPT_VERIFY_MESSAGE_PARA param;
    HCRYPTPROV hCryptProv = 0; /* Дескриптор провайдера*/
    DWORD signed_len = 0;

    memset(&param, 0, sizeof(CRYPT_VERIFY_MESSAGE_PARA));
    param.cbSize = sizeof(CRYPT_VERIFY_MESSAGE_PARA);
    param.dwMsgAndCertEncodingType = TYPE_DER;
    param.hCryptProv = hCryptProv;
    param.pfnGetSignerCertificate = NULL;
    param.pvGetArg = NULL;
    *Msg = (BYTE *)malloc(signed_len = mem_len);
    DWORD dwSignerIndex = 0; /* Используется вцикле если подпись не одна.*/
    BOOL ret = NULL;

    ret = CryptVerifyMessageSignature(
            &param,
            dwSignerIndex,
            mem_tbs, /* подписанное сообщение*/
            mem_len, /* длина*/
            *Msg,    /* если нужно сохранить вложение BYTE *pbDecoded,*/
            Len,     /* куда сохраняет вложение DWORD *pcbDecoded,*/
            NULL);    /* возвращаемый сертификат на котором проверена ЭЦП (PCCERT_CONTEXT *ppSignerCert)*/

    free( Msg );
    if (ret){
        return RetSuccess(); 
    }
    else{
        return RetError("Ошибка проверки подписи");
    }    
}

Napi::Buffer<BYTE> Crypt(const Napi::CallbackInfo &info)
{
    Napi::Buffer<BYTE> buf = info[0].As<Napi::Buffer<BYTE>>();

    BYTE *pbSignature = NULL;
    DWORD signatureLength = 0;
    StructResult ret = doSign(buf.Data(), buf.ByteLength(), &pbSignature, &signatureLength);

    Napi::Buffer<BYTE> OutBuf;
    OutBuf = Napi::Buffer<BYTE>::Copy(info.Env(), pbSignature, signatureLength);
    free(pbSignature);

    return OutBuf;
}

Napi::Buffer<BYTE> Verify(const Napi::CallbackInfo &info)
{
    // Napi::Error::New(info.Env(), "Пример ошибки").ThrowAsJavaScriptException();
    Napi::Buffer<BYTE> buf = info[0].As<Napi::Buffer<BYTE>>();

    BYTE *pbSignature = NULL;
    DWORD signatureLength = 0;
    StructResult ret = doVerify(buf.Data(), buf.ByteLength(), &pbSignature, &signatureLength);

    Napi::Buffer<BYTE> OutBuf;
    OutBuf = Napi::Buffer<BYTE>::Copy(info.Env(), pbSignature, signatureLength);
    free(pbSignature);

    return OutBuf;
}

static Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports["Crypt"] = Napi::Function::New(env, Crypt);
    exports["Verify"] = Napi::Function::New(env, Verify);
    return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)