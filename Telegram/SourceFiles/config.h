/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "core/version.h"
#include "settings.h"

enum {
	MaxSelectedItems = 100,

	LocalEncryptIterCount = 4000, // key derivation iteration count
	LocalEncryptNoPwdIterCount = 4, // key derivation iteration count without pwd (not secure anyway)
	LocalEncryptSaltSize = 32, // 256 bit

	RecentInlineBotsLimit = 10,

	AutoSearchTimeout = 900, // 0.9 secs

	PreloadHeightsCount = 3, // when 3 screens to scroll left make a preload request

	SearchPeopleLimit = 5,

	MaxMessageSize = 4096,

	WebPageUserId = 701000,

	UpdateDelayConstPart = 8 * 3600, // 8 hour min time between update check requests
	UpdateDelayRandPart = 8 * 3600, // 8 hour max - min time between update check requests

	WrongPasscodeTimeout = 1500,

	ChoosePeerByDragTimeout = 1000, // 1 second mouse not moved to choose dialog when dragging a file
};

inline const char *cGUIDStr() {
#ifndef OS_MAC_STORE
	static const char *gGuidStr = "{87A94AB0-E370-4cde-98D3-ACC110C5967D}";
#else // OS_MAC_STORE
	static const char *gGuidStr = "{E51FB841-8C0B-4EF9-9E9E-5A0078567627}";
#endif // OS_MAC_STORE

	return gGuidStr;
}

static const char *UpdatesPublicKey = "\
-----BEGIN RSA PUBLIC KEY-----\n\
MIGJAoGBALUEi8NQfcq/GToD5CdgdNhgj2at2nusoWsHuUdIOGEOehpt2PiQlzt+\n\
qziKJDO8+tPnQV0Nzq6UqZXA0eCT4CvP2jZyLq/xnNzlinQXT+wPu2wqBabRTfoC\n\
TIiLseFjv2zEsXCCkhiaUfAtU3w09yw0/D8vl1/5+N/4mpAic+0VAgMBAAE=\n\
-----END RSA PUBLIC KEY-----\
";

static const char *UpdatesPublicBetaKey = "\
-----BEGIN RSA PUBLIC KEY-----\n\
MIGJAoGBAPgjMkWHsxk1d4NcPC5jyPlEddvOdl3yH+s8xpm8MxCVwhWu5dazkC0Z\n\
v1/0UnkegO4jNkSY3ycDqn+T3NjxNxnL0EsKh7MjinyMUe3ZISzaIyrdq/8v4bvB\n\
/Z1X5Ruw2HacoWo/EVsXY9zCTrY53IRrKy4HQbCOloK2+TBimyX5AgMBAAE=\n\
-----END RSA PUBLIC KEY-----\
";

#if Q_BYTE_ORDER == Q_BIG_ENDIAN
#error "Only little endian is supported!"
#endif // Q_BYTE_ORDER == Q_BIG_ENDIAN

#if (TDESKTOP_ALPHA_VERSION != 0)

// Private key for downloading closed alphas.
#include "../../../DesktopPrivate/alpha_private.h"

#else
static const char *AlphaPrivateKey = "";
#endif

extern QString gKeyFile;
inline const QString &cDataFile() {
	if (!gKeyFile.isEmpty()) return gKeyFile;
	static const QString res(u"data"_q);
	return res;
}

inline const QRegularExpression &cRussianLetters() {
	static QRegularExpression regexp(QString::fromUtf8("[а-яА-ЯёЁ]"));
	return regexp;
}
