START TRANSACTION;

DROP FUNCTION fSDSS;
DROP FUNCTION fSkyVersion;
DROP FUNCTION fRerun;
DROP FUNCTION fRun;
DROP FUNCTION fCamcol;
DROP FUNCTION fField;
DROP FUNCTION fObj;
DROP FUNCTION fObjidFromSDSS;
DROP FUNCTION fObjidFromSDSSWithFF;
DROP FUNCTION fSpecidFromSDSS;
DROP FUNCTION fPlate;
DROP FUNCTION fMJD;
DROP FUNCTION fFiber;
DROP FUNCTION fPhotoStatus;
DROP FUNCTION fPrimTargetN;
DROP FUNCTION fPrimTarget;
DROP FUNCTION fSecTarget;
DROP FUNCTION fSecTargetN;
DROP FUNCTION fInsideMask;
DROP FUNCTION fInsideMaskN;
DROP FUNCTION fSpecZWarning;
DROP FUNCTION fSpecZWarningN;
DROP FUNCTION fImageMask;
DROP FUNCTION fImageMaskN;
DROP FUNCTION fTiMask;
DROP FUNCTION fTiMaskN;
DROP FUNCTION fPhotoModeN;
DROP FUNCTION fPhotoMode;
DROP FUNCTION fPhotoTypeN;
DROP FUNCTION fPhotoType;
DROP FUNCTION fMaskTypeN;
DROP FUNCTION fMaskType;
DROP FUNCTION fFieldQualityN;
DROP FUNCTION fFieldQuality;
DROP FUNCTION fPspStatus;
DROP FUNCTION fPspStatusN;
DROP FUNCTION fFramesStatus;
DROP FUNCTION fFramesStatusN;
DROP FUNCTION fSpecClass;
DROP FUNCTION fSpecClassN;
DROP FUNCTION fSpecLineNames;
DROP FUNCTION fSpecLineNamesN;
DROP FUNCTION fSpecZStatusN;
DROP FUNCTION fSpecZStatus;
DROP FUNCTION fHoleType;
DROP FUNCTION fHoleTypeN;
DROP FUNCTION fObjType;
DROP FUNCTION fObjTypeN;
DROP FUNCTION fProgramType;
DROP FUNCTION fProgramTypeN;
DROP FUNCTION fCoordType;
DROP FUNCTION fCoordTypeN;
DROP FUNCTION fFieldMask;
DROP FUNCTION fFieldMaskN;
DROP FUNCTION fMJDToGMT;
DROP FUNCTION fDistanceArcMinXYZ;
DROP FUNCTION fDistanceArcMinEq;
DROP FUNCTION fIAUFromEq;
DROP FUNCTION fDMS;
DROP FUNCTION fHMS;
DROP FUNCTION fDMSbase;
DROP FUNCTION fHMSbase;
DROP FUNCTION fMagToFluxErr;
DROP FUNCTION fMagToFlux;
DROP FUNCTION fEtaToNormal;
DROP FUNCTION fStripeToNormal;
DROP FUNCTION fGetLat;
DROP FUNCTION fGetLon;
DROP FUNCTION fGetLonLat;
DROP FUNCTION fEqFromMuNu;
DROP FUNCTION fCoordsFromEq;
DROP FUNCTION fMuFromEq;
DROP FUNCTION fNuFromEq;
DROP FUNCTION fEtaFromEq;
DROP FUNCTION fLambdaFromEq;
DROP FUNCTION fMuNuFromEq;
DROP FUNCTION fWedgeV3;
DROP FUNCTION fRotateV3;
DROP FUNCTION fTokenStringToTable;
DROP FUNCTION fNormalizeString;
DROP FUNCTION fTokenAdvance;
DROP FUNCTION fTokenNext;
DROP FUNCTION fReplace;
DROP FUNCTION fIsNumbers;
DROP FUNCTION fHtmToString;
DROP FUNCTION fHtmXyz;
DROP FUNCTION fHtmEq;
DROP FUNCTION fHtmLookupEq;
DROP FUNCTION fHtmLookupXyz;
DROP FUNCTION fPhotoDescription;
DROP FUNCTION fPhotoStatusN;
DROP FUNCTION fPhotoFlagsN;
DROP FUNCTION fPhotoFlags;
DROP FUNCTION fStripeOfRun;
DROP FUNCTION fStripOfRun;
DROP FUNCTION fGetDiagChecksum;

COMMIT;
