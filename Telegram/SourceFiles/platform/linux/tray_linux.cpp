/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "platform/linux/tray_linux.h"

#include "kotato/kotato_settings.h"
#include "base/invoke_queued.h"
#include "base/qt_signal_producer.h"
#include "core/application.h"
#include "core/sandbox.h"
#include "platform/platform_specific.h"
#include "ui/ui_utility.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_controller.h"
#include "styles/style_window.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSystemTrayIcon>

namespace Platform {

class IconGraphic final {
public:
	explicit IconGraphic();
	~IconGraphic();

	[[nodiscard]] bool isRefreshNeeded(
		const QIcon &systemIcon,
		const QString &iconThemeName,
		int counter,
		bool muted) const;
	[[nodiscard]] QIcon systemIcon(
		const QString &iconThemeName,
		int counter,
		bool muted) const;
	[[nodiscard]] QIcon trayIcon(
		const QIcon &systemIcon,
		const QString &iconThemeName,
		int counter,
		bool muted);

private:
	[[nodiscard]] QString panelIconName(int counter, bool muted) const;
	[[nodiscard]] int counterSlice(int counter) const;
	[[nodiscard]] bool useIconFromTheme(const QString &iconName) const;
	void updateIconRegenerationNeeded(
		const QIcon &icon,
		const QIcon &systemIcon,
		const QString &iconThemeName,
		int counter,
		bool muted);
	[[nodiscard]] QSize dprSize(const QImage &image) const;

	const QString _panelTrayIconName;
	const QString _mutePanelTrayIconName;
	const QString _attentionPanelTrayIconName;
	const QString _telegramPanelTrayIconName;
	const QString _telegramMutePanelTrayIconName;
	const QString _telegramAttentionPanelTrayIconName;

	const int _iconSizes[7];

	bool _muted = true;
	int32 _count = 0;
	base::flat_map<int, QImage> _imageBack;
	QIcon _trayIcon;
	QIcon _systemIcon;
	QString _themeName;
	int _customId = 0;
	bool _counterDisabled = false;

};

IconGraphic::IconGraphic()
: _panelTrayIconName("kotatogram-panel")
, _mutePanelTrayIconName("kotatogram-mute-panel")
, _attentionPanelTrayIconName("kotatogram-attention-panel")
, _telegramPanelTrayIconName("telegram-panel")
, _telegramMutePanelTrayIconName("telegram-mute-panel")
, _telegramAttentionPanelTrayIconName("telegram-attention-panel")
, _iconSizes{ 16, 22, 32, 48, 64, 128, 256 } {
}

IconGraphic::~IconGraphic() = default;

QString IconGraphic::panelIconName(int counter, bool muted) const {
	const auto useTelegramPanelIcon = ::Kotato::JsonSettings::GetBool("use_telegram_panel_icon");
	return (counter > 0)
		? (muted
			? (useTelegramPanelIcon
				? _telegramMutePanelTrayIconName
				: _mutePanelTrayIconName)
			: (useTelegramPanelIcon
				? _telegramAttentionPanelTrayIconName
				: _attentionPanelTrayIconName))
		: (useTelegramPanelIcon
			? _telegramPanelTrayIconName
			: _panelTrayIconName);
}

QIcon IconGraphic::systemIcon(
		const QString &iconThemeName,
		int counter,
		bool muted) const {
	if (iconThemeName == _themeName
		&& (counter > 0) == (_count > 0)
		&& muted == _muted) {
		return _systemIcon;
	}

	const auto candidates = {
		panelIconName(counter, muted),
		base::IconName(),
	};

	for (const auto &candidate : candidates) {
		const auto icon = QIcon::fromTheme(candidate);
		if (icon.name() == candidate) {
			return icon;
		}
	}

	return QIcon();
}


int IconGraphic::counterSlice(int counter) const {
	return (counter >= 1000)
		? (1000 + (counter % 100))
		: counter;
}

bool IconGraphic::useIconFromTheme(const QString &iconName) const {
	return ::Kotato::JsonSettings::GetBool("disable_tray_counter")
		&& !QFileInfo::exists(cWorkingDir() + "tdata/icon.png")
		&& ::Kotato::JsonSettings::GetInt("custom_app_icon") == 0
		&& !iconName.isEmpty();
}

bool IconGraphic::isRefreshNeeded(
		const QIcon &systemIcon,
		const QString &iconThemeName,
		int counter,
		bool muted) const {
	return _trayIcon.isNull()
		|| iconThemeName != _themeName
		|| systemIcon.name() != _systemIcon.name()
		|| muted != _muted
		|| counterSlice(counter) != _count
		|| ::Kotato::JsonSettings::GetInt("custom_app_icon") != _customId
		|| ::Kotato::JsonSettings::GetBool("disable_tray_counter") != _counterDisabled;
}

void IconGraphic::updateIconRegenerationNeeded(
		const QIcon &icon,
		const QIcon &systemIcon,
		const QString &iconThemeName,
		int counter,
		bool muted) {
	_trayIcon = icon;
	_systemIcon = systemIcon;
	_themeName = iconThemeName;
	_count = counterSlice(counter);
	_muted = muted;
	_customId = ::Kotato::JsonSettings::GetInt("custom_app_icon");
	_counterDisabled = ::Kotato::JsonSettings::GetBool("disable_tray_counter");
}

QSize IconGraphic::dprSize(const QImage &image) const {
	return image.size() / image.devicePixelRatio();
}

QIcon IconGraphic::trayIcon(
		const QIcon &systemIcon,
		const QString &iconThemeName,
		int counter,
		bool muted) {
	if (!isRefreshNeeded(systemIcon, iconThemeName, counter, muted)) {
		return _trayIcon;
	}

	if (useIconFromTheme(systemIcon.name())) {
		updateIconRegenerationNeeded(
			systemIcon,
			systemIcon,
			iconThemeName,
			counter,
			muted);

		return systemIcon;
	}

	QIcon result;
	const auto customAppIcon = ::Kotato::JsonSettings::GetInt("custom_app_icon");
	const auto disableTrayCounter = ::Kotato::JsonSettings::GetInt("disable_tray_counter");

	for (const auto iconSize : _iconSizes) {
		auto &currentImageBack = _imageBack[iconSize];
		const auto desiredSize = QSize(iconSize, iconSize);

		if (currentImageBack.isNull()
			|| iconThemeName != _themeName
			|| systemIcon.name() != _systemIcon.name()
			|| customAppIcon != _customId
			|| disableTrayCounter != _counterDisabled) {
			if (QFileInfo::exists(cWorkingDir() + "tdata/icon.png")) {
				currentImageBack = QImage(cWorkingDir() + "tdata/icon.png");
			} else if (customAppIcon != 0) {
				currentImageBack = Window::Logo(customAppIcon);
			} else if (!systemIcon.isNull()) {
				// We can't use QIcon::actualSize here
				// since it works incorrectly with svg icon themes
				currentImageBack = systemIcon
					.pixmap(desiredSize)
					.toImage();

				const auto firstAttemptSize = dprSize(currentImageBack);

				// if current icon theme is not a svg one, Qt can return
				// a pixmap that less in size even if there are a bigger one
				if (firstAttemptSize.width() < desiredSize.width()) {
					const auto availableSizes = systemIcon.availableSizes();

					const auto biggestSize = ranges::max_element(
						availableSizes,
						std::less<>(),
						&QSize::width);

					if (biggestSize->width() > firstAttemptSize.width()) {
						currentImageBack = systemIcon
							.pixmap(*biggestSize)
							.toImage();
					}
				}
			} else {
				currentImageBack = Window::Logo();
			}

			if (dprSize(currentImageBack) != desiredSize) {
				currentImageBack = currentImageBack.scaled(
					desiredSize * currentImageBack.devicePixelRatio(),
					Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation);
			}
		}

		result.addPixmap(Ui::PixmapFromImage((!disableTrayCounter && counter > 0)
			? Window::WithSmallCounter(std::move(currentImageBack), {
				.size = iconSize,
				.count = counter,
				.bg = muted ? st::trayCounterBgMute : st::trayCounterBg,
				.fg = st::trayCounterFg,
			}) : std::move(currentImageBack)));
	}

	updateIconRegenerationNeeded(
		result,
		systemIcon,
		iconThemeName,
		counter,
		muted);

	return result;
}

class TrayEventFilter final : public QObject {
public:
	TrayEventFilter(not_null<QObject*> parent);

	[[nodiscard]] rpl::producer<> contextMenuFilters() const;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	const QString _iconObjectName;
	rpl::event_stream<> _contextMenuFilters;

};

TrayEventFilter::TrayEventFilter(not_null<QObject*> parent)
: QObject(parent)
, _iconObjectName("QSystemTrayIconSys") {
	parent->installEventFilter(this);
}

bool TrayEventFilter::eventFilter(QObject *obj, QEvent *event) {
	if (event->type() == QEvent::MouseButtonPress
		&& obj->objectName() == _iconObjectName) {
		const auto m = static_cast<QMouseEvent*>(event);
		if (m->button() == Qt::RightButton) {
			Core::Sandbox::Instance().customEnterFromEventLoop([&] {
				_contextMenuFilters.fire({});
			});
			return true;
		}
	}
	return false;
}

rpl::producer<> TrayEventFilter::contextMenuFilters() const {
	return _contextMenuFilters.events();
}

Tray::Tray() {
	LOG(("System tray available: %1").arg(Logs::b(TrayIconSupported())));
}

void Tray::createIcon() {
	if (!_icon) {
		if (!_iconGraphic) {
			_iconGraphic = std::make_unique<IconGraphic>();
		}

		const auto showXEmbed = [=] {
			_aboutToShowRequests.fire({});
			InvokeQueued(_menuXEmbed.get(), [=] {
				_menuXEmbed->popup(QCursor::pos());
			});
		};

		const auto iconThemeName = QIcon::themeName();
		const auto counter = Core::App().unreadBadge();
		const auto muted = Core::App().unreadBadgeMuted();
		static const auto workdir = QDir::toNativeSeparators(QDir::cleanPath(cWorkingDir()));

		_icon = base::make_unique_q<QSystemTrayIcon>(nullptr);
		_icon->setIcon(_iconGraphic->trayIcon(
			_iconGraphic->systemIcon(
				iconThemeName,
				counter,
				muted),
			iconThemeName,
			counter,
			muted));
		_icon->setToolTip(AppName.utf16()+"\n"+workdir);

		using Reason = QSystemTrayIcon::ActivationReason;
		base::qt_signal_producer(
			_icon.get(),
			&QSystemTrayIcon::activated
		) | rpl::start_with_next([=](Reason reason) {
			if (reason == QSystemTrayIcon::Context) {
				showXEmbed();
			} else {
				_iconClicks.fire({});
			}
		}, _lifetime);

		_icon->setContextMenu(_menu.get());

		if (!_eventFilter) {
			_eventFilter = base::make_unique_q<TrayEventFilter>(
				QCoreApplication::instance());
			_eventFilter->contextMenuFilters(
			) | rpl::start_with_next([=] {
				showXEmbed();
			}, _lifetime);
		}
	}
	updateIcon();

	_icon->show();
}

void Tray::destroyIcon() {
	_icon = nullptr;
}

void Tray::updateIcon() {
	if (!_icon || !_iconGraphic) {
		return;
	}
	const auto counter = Core::App().unreadBadge();
	const auto muted = Core::App().unreadBadgeMuted();
	const auto iconThemeName = QIcon::themeName();
	const auto systemIcon = _iconGraphic->systemIcon(
		iconThemeName,
		counter,
		muted);

	if (_iconGraphic->isRefreshNeeded(
		systemIcon,
		iconThemeName,
		counter,
		muted)) {
		_icon->setIcon(_iconGraphic->trayIcon(
			systemIcon,
			iconThemeName,
			counter,
			muted));
	}
}

void Tray::createMenu() {
	if (!_menu) {
		_menu = base::make_unique_q<QMenu>(nullptr);
	}
	if (!_menuXEmbed) {
		_menuXEmbed = base::make_unique_q<Ui::PopupMenu>(nullptr);
		_menuXEmbed->deleteOnHide(false);
	}
}

void Tray::destroyMenu() {
	_menuXEmbed = nullptr;
	if (_menu) {
		_menu->clear();
	}
	_actionsLifetime.destroy();
}

void Tray::addAction(rpl::producer<QString> text, Fn<void()> &&callback) {
	if (_menuXEmbed) {
		const auto XEAction = _menuXEmbed->addAction(QString(), callback);
		rpl::duplicate(
			text
		) | rpl::start_with_next([=](const QString &text) {
			XEAction->setText(text);
		}, _actionsLifetime);
	}

	if (_menu) {
		const auto action = _menu->addAction(QString(), std::move(callback));
		std::move(
			text
		) | rpl::start_with_next([=](const QString &text) {
			action->setText(text);
		}, _actionsLifetime);
	}
}

void Tray::showTrayMessage() const {
}

bool Tray::hasTrayMessageSupport() const {
	return false;
}

rpl::producer<> Tray::aboutToShowRequests() const {
	return rpl::merge(
		_aboutToShowRequests.events(),
		_menu
			? base::qt_signal_producer(_menu.get(), &QMenu::aboutToShow)
			: rpl::never<>() | rpl::type_erased());
}

rpl::producer<> Tray::showFromTrayRequests() const {
	return rpl::never<>();
}

rpl::producer<> Tray::hideToTrayRequests() const {
	return rpl::never<>();
}

rpl::producer<> Tray::iconClicks() const {
	return _iconClicks.events();
}

bool Tray::hasIcon() const {
	return _icon;
}

rpl::lifetime &Tray::lifetime() {
	return _lifetime;
}

Tray::~Tray() = default;

} // namespace Platform
