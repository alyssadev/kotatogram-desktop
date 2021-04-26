/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/filters/edit_filter_chats_list.h"

#include "history/history.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "base/object_ptr.h"
#include "styles/style_window.h"
#include "styles/style_boxes.h"

namespace {

constexpr auto kMaxExceptions = 100;

using Flag = Data::ChatFilter::Flag;
using Flags = Data::ChatFilter::Flags;

constexpr auto kAllTypes = {
	Flag::Contacts,
	Flag::NonContacts,
	Flag::Groups,
	Flag::Channels,
	Flag::Bots,
	Flag::NoMuted,
	Flag::NoRead,
	Flag::NoArchived,
	Flag::Owned,
	Flag::Admin,
	Flag::NotOwned,
	Flag::NotAdmin,
	Flag::Recent,
	Flag::NoFilter,
};

struct RowSelectionChange {
	not_null<PeerListRow*> row;
	bool checked = false;
};

class TypeRow final : public PeerListRow {
public:
	explicit TypeRow(Flag flag);

	QString generateName() override;
	QString generateShortName() override;
	PaintRoundImageCallback generatePaintUserpicCallback() override;

private:
	[[nodiscard]] Flag flag() const;

};

class ExceptionRow final : public ChatsListBoxController::Row {
public:
	explicit ExceptionRow(not_null<History*> history);

	QString generateName() override;
	QString generateShortName() override;
	PaintRoundImageCallback generatePaintUserpicCallback() override;

};

class TypeController final : public PeerListController {
public:
	TypeController(
		not_null<Main::Session*> session,
		Flags options,
		Flags selected);

	Main::Session &session() const override;
	void prepare() override;
	void rowClicked(not_null<PeerListRow*> row) override;

	[[nodiscard]] rpl::producer<Flags> selectedChanges() const;
	[[nodiscard]] auto rowSelectionChanges() const
		-> rpl::producer<RowSelectionChange>;

private:
	[[nodiscard]] std::unique_ptr<PeerListRow> createRow(Flag flag) const;
	[[nodiscard]] Flags collectSelectedOptions() const;

	const not_null<Main::Session*> _session;
	Flags _options;

	rpl::event_stream<> _selectionChanged;
	rpl::event_stream<RowSelectionChange> _rowSelectionChanges;

};

[[nodiscard]] object_ptr<Ui::RpWidget> CreateSectionSubtitle(
		not_null<QWidget*> parent,
		rpl::producer<QString> text) {
	auto result = object_ptr<Ui::FixedHeightWidget>(
		parent,
		st::searchedBarHeight);

	const auto raw = result.data();
	raw->paintRequest(
	) | rpl::start_with_next([=](QRect clip) {
		auto p = QPainter(raw);
		p.fillRect(clip, st::searchedBarBg);
	}, raw->lifetime());

	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		raw,
		std::move(text),
		st::windowFilterChatsSectionSubtitle);
	raw->widthValue(
	) | rpl::start_with_next([=](int width) {
		const auto padding = st::windowFilterChatsSectionSubtitlePadding;
		const auto available = width - padding.left() - padding.right();
		label->resizeToNaturalWidth(available);
		label->moveToLeft(padding.left(), padding.top(), width);
	}, label->lifetime());

	return result;
}

[[nodiscard]] uint64 TypeId(Flag flag) {
	return PeerId(FakeChatId(static_cast<BareId>(flag))).value;
}

TypeRow::TypeRow(Flag flag) : PeerListRow(TypeId(flag)) {
}

QString TypeRow::generateName() {
	return FilterChatsTypeName(flag());
}

QString TypeRow::generateShortName() {
	return generateName();
}

PaintRoundImageCallback TypeRow::generatePaintUserpicCallback() {
	const auto flag = this->flag();
	return [=](Painter &p, int x, int y, int outerWidth, int size) {
		PaintFilterChatsTypeIcon(p, flag, x, y, outerWidth, size);
	};
}

Flag TypeRow::flag() const {
	return static_cast<Flag>(id() & 0xFFFF);
}

ExceptionRow::ExceptionRow(not_null<History*> history) : Row(history) {
	if (peer()->isSelf()) {
		setCustomStatus(tr::lng_saved_forward_here(tr::now));
	}
}

QString ExceptionRow::generateName() {
	return peer()->isSelf()
		? tr::lng_saved_messages(tr::now)
		: peer()->isRepliesChat()
		? tr::lng_replies_messages(tr::now)
		: Row::generateName();
}

QString ExceptionRow::generateShortName() {
	return generateName();
}

PaintRoundImageCallback ExceptionRow::generatePaintUserpicCallback() {
	const auto peer = this->peer();
	const auto saved = peer->isSelf();
	const auto replies = peer->isRepliesChat();
	auto userpic = saved ? nullptr : ensureUserpicView();
	return [=](Painter &p, int x, int y, int outerWidth, int size) mutable {
		if (saved) {
			Ui::EmptyUserpic::PaintSavedMessages(p, x, y, outerWidth, size);
		} else if (replies) {
			Ui::EmptyUserpic::PaintRepliesMessages(p, x, y, outerWidth, size);
		} else {
			peer->paintUserpicLeft(p, userpic, x, y, outerWidth, size);
		}
	};
}

TypeController::TypeController(
	not_null<Main::Session*> session,
	Flags options,
	Flags selected)
: _session(session)
, _options(options) {
}

Main::Session &TypeController::session() const {
	return *_session;
}

void TypeController::prepare() {
	for (const auto flag : kAllTypes) {
		if (_options & flag) {
			delegate()->peerListAppendRow(createRow(flag));
		}
	}
	delegate()->peerListRefreshRows();
}

Flags TypeController::collectSelectedOptions() const {
	auto result = Flags();
	for (const auto flag : kAllTypes) {
		if (const auto row = delegate()->peerListFindRow(TypeId(flag))) {
			if (row->checked()) {
				result |= flag;
			}
		}
	}
	return result;
}

void TypeController::rowClicked(not_null<PeerListRow*> row) {
	const auto checked = !row->checked();
	delegate()->peerListSetRowChecked(row, checked);
	_rowSelectionChanges.fire({ row, checked });
}

std::unique_ptr<PeerListRow> TypeController::createRow(Flag flag) const {
	return std::make_unique<TypeRow>(flag);
}

rpl::producer<Flags> TypeController::selectedChanges() const {
	return _rowSelectionChanges.events(
	) | rpl::map([=] {
		return collectSelectedOptions();
	});
}

auto TypeController::rowSelectionChanges() const
-> rpl::producer<RowSelectionChange> {
	return _rowSelectionChanges.events();
}

} // namespace

[[nodiscard]] QString FilterChatsTypeName(Flag flag) {
	switch (flag) {
	case Flag::Contacts: return tr::lng_filters_type_contacts(tr::now);
	case Flag::NonContacts:
		return tr::lng_filters_type_non_contacts(tr::now);
	case Flag::Groups: return tr::lng_filters_type_groups(tr::now);
	case Flag::Channels: return tr::lng_filters_type_channels(tr::now);
	case Flag::Bots: return tr::lng_filters_type_bots(tr::now);
	case Flag::NoMuted: return tr::lng_filters_type_no_muted(tr::now);
	case Flag::NoArchived: return tr::lng_filters_type_no_archived(tr::now);
	case Flag::NoRead: return tr::lng_filters_type_no_read(tr::now);
	case Flag::Owned: return tr::ktg_filters_exclude_not_owned(tr::now);
	case Flag::Admin: return tr::ktg_filters_exclude_not_admin(tr::now);
	case Flag::NotOwned: return tr::ktg_filters_exclude_owned(tr::now);
	case Flag::NotAdmin: return tr::ktg_filters_exclude_admin(tr::now);
	case Flag::Recent: return tr::ktg_filters_exclude_not_recent(tr::now);
	case Flag::NoFilter: return tr::ktg_filters_exclude_filtered(tr::now);
	}
	Unexpected("Flag in TypeName.");
}

void PaintFilterChatsTypeIcon(
		Painter &p,
		Data::ChatFilter::Flag flag,
		int x,
		int y,
		int outerWidth,
		int size) {
	const auto &color = [&]() -> const style::color& {
		switch (flag) {
		case Flag::Contacts: return st::historyPeer4UserpicBg;
		case Flag::NonContacts: return st::historyPeer7UserpicBg;
		case Flag::Groups: return st::historyPeer2UserpicBg;
		case Flag::Channels: return st::historyPeer1UserpicBg;
		case Flag::Bots: return st::historyPeer6UserpicBg;
		case Flag::NoMuted: return st::historyPeer6UserpicBg;
		case Flag::NoArchived: return st::historyPeer4UserpicBg;
		case Flag::NoRead: return st::historyPeer7UserpicBg;
		case Flag::Owned: return st::historyPeer2UserpicBg;
		case Flag::Admin: return st::historyPeer3UserpicBg;
		case Flag::NotOwned: return st::historyPeer2UserpicBg;
		case Flag::NotAdmin: return st::historyPeer3UserpicBg;
		case Flag::Recent: return st::historyPeer6UserpicBg;
		case Flag::NoFilter: return st::historyPeer7UserpicBg;
		}
		Unexpected("Flag in color paintFlagIcon.");
	}();
	const auto &icon = [&]() -> const style::icon& {
		switch (flag) {
		case Flag::Contacts: return st::windowFilterTypeContacts;
		case Flag::NonContacts: return st::windowFilterTypeNonContacts;
		case Flag::Groups: return st::windowFilterTypeGroups;
		case Flag::Channels: return st::windowFilterTypeChannels;
		case Flag::Bots: return st::windowFilterTypeBots;
		case Flag::NoMuted: return st::windowFilterTypeNoMuted;
		case Flag::NoArchived: return st::windowFilterTypeNoArchived;
		case Flag::NoRead: return st::windowFilterTypeNoRead;
		case Flag::Owned: return st::windowFilterTypeOwned;
		case Flag::Admin: return st::windowFilterTypeAdmin;
		case Flag::NotOwned: return st::windowFilterTypeNotOwned;
		case Flag::NotAdmin: return st::windowFilterTypeNotAdmin;
		case Flag::Recent: return st::windowFilterTypeRecent;
		case Flag::NoFilter: return st::windowFilterTypeNoFilter;
		}
		Unexpected("Flag in icon paintFlagIcon.");
	}();
	const auto rect = style::rtlrect(x, y, size, size, outerWidth);
	auto hq = PainterHighQualityEnabler(p);
	p.setBrush(color->b);
	p.setPen(Qt::NoPen);
	switch (cUserpicCornersType()) {
		case 0:
			p.drawRoundedRect(
				rect,
				0, 0);
			break;

		case 1:
			p.drawRoundedRect(
				rect,
				st::buttonRadius, st::buttonRadius);
			break;

		case 2:
			p.drawRoundedRect(
				rect,
				st::dateRadius, st::dateRadius);
			break;

		default:
			p.drawEllipse(rect);
	}
	icon.paintInCenter(p, rect);
}

EditFilterChatsListController::EditFilterChatsListController(
	not_null<Main::Session*> session,
	rpl::producer<QString> title,
	Flags options,
	Flags selected,
	const base::flat_set<not_null<History*>> &peers,
	bool isLocal)
: ChatsListBoxController(session)
, _session(session)
, _title(std::move(title))
, _peers(peers)
, _options(options)
, _selected(selected)
, _isLocal(isLocal) {
}

Main::Session &EditFilterChatsListController::session() const {
	return *_session;
}

void EditFilterChatsListController::rowClicked(not_null<PeerListRow*> row) {
	const auto count = delegate()->peerListSelectedRowsCount();
	if (count < kMaxExceptions || row->checked() || _isLocal) {
		delegate()->peerListSetRowChecked(row, !row->checked());
		updateTitle();
	}
}

void EditFilterChatsListController::itemDeselectedHook(
		not_null<PeerData*> peer) {
	updateTitle();
}

bool EditFilterChatsListController::isForeignRow(PeerListRowId itemId) {
	return ranges::contains(kAllTypes, itemId, TypeId);
}

bool EditFilterChatsListController::handleDeselectForeignRow(
		PeerListRowId itemId) {
	if (isForeignRow(itemId)) {
		_deselectOption(itemId);
		return true;
	}
	return false;
}

void EditFilterChatsListController::prepareViewHook() {
	delegate()->peerListSetTitle(std::move(_title));
	delegate()->peerListSetAboveWidget(prepareTypesList());

	const auto count = int(_peers.size());
	const auto rows = std::make_unique<std::optional<ExceptionRow>[]>(count);
	auto i = 0;
	for (const auto history : _peers) {
		rows[i++].emplace(history);
	}
	auto pointers = std::vector<ExceptionRow*>();
	pointers.reserve(count);
	for (auto i = 0; i != count; ++i) {
		pointers.push_back(&*rows[i]);
	}
	delegate()->peerListAddSelectedRows(pointers);
	updateTitle();
}

object_ptr<Ui::RpWidget> EditFilterChatsListController::prepareTypesList() {
	auto result = object_ptr<Ui::VerticalLayout>((QWidget*)nullptr);
	const auto container = result.data();
	container->add(CreateSectionSubtitle(
		container,
		tr::lng_filters_edit_types()));
	container->add(object_ptr<Ui::FixedHeightWidget>(
		container,
		st::membersMarginTop));
	const auto delegate = container->lifetime().make_state<
		PeerListContentDelegateSimple
	>();
	const auto controller = container->lifetime().make_state<TypeController>(
		&session(),
		_options,
		_selected);
	controller->setStyleOverrides(&st::windowFilterSmallList);
	const auto content = result->add(object_ptr<PeerListContent>(
		container,
		controller));
	delegate->setContent(content);
	controller->setDelegate(delegate);
	for (const auto flag : kAllTypes) {
		if (_selected & flag) {
			if (const auto row = delegate->peerListFindRow(TypeId(flag))) {
				content->changeCheckState(row, true, anim::type::instant);
				this->delegate()->peerListSetForeignRowChecked(
					row,
					true,
					anim::type::instant);
			}
		}
	}
	container->add(object_ptr<Ui::FixedHeightWidget>(
		container,
		st::membersMarginBottom));
	container->add(CreateSectionSubtitle(
		container,
		tr::lng_filters_edit_chats()));

	controller->selectedChanges(
	) | rpl::start_with_next([=](Flags selected) {
		_selected = selected;
	}, _lifetime);

	controller->rowSelectionChanges(
	) | rpl::start_with_next([=](RowSelectionChange update) {
		this->delegate()->peerListSetForeignRowChecked(
			update.row,
			update.checked,
			anim::type::normal);
	}, _lifetime);

	_deselectOption = [=](PeerListRowId itemId) {
		if (const auto row = delegate->peerListFindRow(itemId)) {
			delegate->peerListSetRowChecked(row, false);
		}
	};

	return result;
}

auto EditFilterChatsListController::createRow(not_null<History*> history)
-> std::unique_ptr<Row> {
	return history->inChatList()
		? std::make_unique<ExceptionRow>(history)
		: nullptr;
}

void EditFilterChatsListController::updateTitle() {
	auto types = 0;
	for (const auto flag : kAllTypes) {
		if (_selected & flag) {
			++types;
		}
	}
	const auto count = delegate()->peerListSelectedRowsCount() - types;
	const auto additional = _isLocal
		? tr::lng_filters_chats_count(tr::now, lt_count_short, count)
		: qsl("%1 / %2").arg(count).arg(kMaxExceptions);
	delegate()->peerListSetAdditionalTitle(rpl::single(additional));
}
