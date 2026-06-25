#include "runpod_ico_classifier/runpod_ico_classifier.hpp"

#include "runpod_ico_classifier/pick_dialogs.hpp"

#include <gdk/gdkkeysyms.h>
#include <gtkmm/messagedialog.h>

#include <filesystem>
#include <sstream>

namespace runpod_ico_classifier {

namespace {

bool ensure_domain(AppConfig &config) {
    while (true) {
        if (!config.domain.empty()) {
            std::string error;
            if (check_server_health(config.domain, error))
                return true;

            Gtk::MessageDialog warn(
                "Server unavailable:\n" + error +
                    "\n\nCheck the API address and try again.",
                false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK);
            warn.run();
        }

        DomainDialog dialog(config.domain);
        if (dialog.run() != Gtk::RESPONSE_OK)
            return false;

        config.domain = normalize_domain(dialog.domain());
        if (config.domain.empty()) {
            Gtk::MessageDialog empty("Server address cannot be empty.", false,
                                     Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
            empty.run();
            continue;
        }

        std::string error;
        if (!check_server_health(config.domain, error)) {
            Gtk::MessageDialog warn(
                "Server unavailable:\n" + error +
                    "\n\nCheck the API address and try again.",
                false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK);
            warn.run();
            continue;
        }

        if (!save_config(config, error)) {
            Gtk::MessageDialog save_error("Failed to save settings:\n" + error,
                                          false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
            save_error.run();
        }
        return true;
    }
}

Glib::RefPtr<Gdk::Pixbuf> stock_pixbuf(const Gtk::StockID &stock_id) {
    Glib::RefPtr<Gtk::IconSet> iconset;
    if (!Gtk::Stock::lookup(stock_id, iconset))
        return {};
    Gtk::Image image;
    return iconset->render_icon_pixbuf(image.get_style_context(), Gtk::ICON_SIZE_MENU);
}

} // namespace

RunpodIcoClassifier::RunpodIcoClassifier(const Glib::RefPtr<Gtk::Application> &app,
                                         const AppConfig &config)
    : Gtk::ApplicationWindow(app),
      app_(app),
      config_(config),
      menu_file_("_File"),
      menu_open_("_Open Folder..."),
      menu_quit_("_Quit"),
      menu_service_("_Service"),
      menu_domain_("_Server Address..."),
      menu_check_("_Check"),
      menu_check_all_("Check _All"),
      tool_open_(),
      tool_check_(),
      tree_view_(),
      treestore_icons_(Gtk::TreeStore::create(icon_cols_)),
      check_finished_dispatcher_() {
    set_title("Icon Checker");
    setup_stock_icons();
    setup_ui();
    setup_menu_bar();
    setup_toolbar();
    setup_center_pane();
    setup_status_bar();
    setup_signals();
    setup_accel_groups();
    setup_data();
    show_all_children();
}

void RunpodIcoClassifier::setup_stock_icons() {
    pixbuf_check_ = stock_pixbuf(Gtk::Stock::YES);
    pixbuf_cross_ = stock_pixbuf(Gtk::Stock::NO);
    pixbuf_warning_ = stock_pixbuf(Gtk::Stock::DIALOG_WARNING);
    pixbuf_pending_ = stock_pixbuf(Gtk::Stock::DIALOG_QUESTION);
}

void RunpodIcoClassifier::setup_ui() {
    add(root_box_);
    set_border_width(0);
    set_default_size(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    set_size_request(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
}

void RunpodIcoClassifier::setup_menu_bar() {
    menu_file_.set_use_underline(true);
    menu_open_.set_use_underline(true);
    menu_quit_.set_use_underline(true);
    menu_service_.set_use_underline(true);
    menu_domain_.set_use_underline(true);
    menu_check_.set_use_underline(true);
    menu_check_all_.set_use_underline(true);

    menu_file_sub_.append(menu_open_);
    menu_file_sub_.append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());
    menu_file_sub_.append(menu_quit_);
    menu_file_.set_submenu(menu_file_sub_);

    menu_service_sub_.append(menu_domain_);
    menu_service_.set_submenu(menu_service_sub_);

    menu_check_sub_.append(menu_check_all_);
    menu_check_.set_submenu(menu_check_sub_);

    menu_bar_.append(menu_file_);
    menu_bar_.append(menu_check_);
    menu_bar_.append(menu_service_);
    root_box_.pack_start(menu_bar_, Gtk::PACK_SHRINK);
}

void RunpodIcoClassifier::setup_toolbar() {
    tool_open_.set_stock_id(Gtk::Stock::OPEN);
    tool_open_.set_label("Open Folder");
    tool_open_.set_tooltip_text("Select a folder with PNG icons");
    tool_check_.set_stock_id(Gtk::Stock::EXECUTE);
    tool_check_.set_label("Check All");
    tool_check_.set_tooltip_text("Send icons to the API server");
    toolbar_.set_icon_size(Gtk::ICON_SIZE_SMALL_TOOLBAR);
    toolbar_.insert(tool_open_, 0);
    toolbar_.insert(toolbar_sep_, 1);
    toolbar_.insert(tool_check_, 2);
    root_box_.pack_start(toolbar_, Gtk::PACK_SHRINK);
}

void RunpodIcoClassifier::setup_center_pane() {
    tree_scroller_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    tree_scroller_.set_shadow_type(Gtk::SHADOW_IN);
    tree_scroller_.set_margin_bottom(5);
    tree_scroller_.set_min_content_width(MIN_TREE_WIDTH);

    tree_view_.set_model(treestore_icons_);
    tree_view_.set_headers_visible(true);
    tree_view_.set_enable_tree_lines(true);
    tree_view_.set_hexpand(true);
    tree_view_.set_vexpand(true);

    int col_index = 0;
    {
        auto renderer = Gtk::make_managed<Gtk::CellRendererPixbuf>();
        auto column = Gtk::make_managed<Gtk::TreeViewColumn>("", *renderer);
        column->add_attribute(renderer->property_pixbuf(), icon_cols_.status_pixbuf);
        column->set_fixed_width(36);
        column->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
        tree_view_.append_column(*column);
        ++col_index;
    }
    tree_view_.append_column("File", icon_cols_.filename);
    tree_view_.append_column("Expected", icon_cols_.expected);
    tree_view_.append_column("Predicted", icon_cols_.predicted);
    tree_view_.append_column("Confidence", icon_cols_.confidence);
    tree_view_.append_column("Comment", icon_cols_.details);
    tree_view_.get_column(col_index)->set_expand(true);

    for (auto *column : tree_view_.get_columns()) {
        if (column)
            column->set_resizable(true);
    }

    tree_scroller_.add(tree_view_);
    root_box_.pack_start(tree_scroller_, Gtk::PACK_EXPAND_WIDGET);
}

void RunpodIcoClassifier::setup_status_bar() {
    status_context_id_ = status_bar_.get_context_id("main");
    root_box_.pack_start(status_bar_, Gtk::PACK_SHRINK);
}

void RunpodIcoClassifier::setup_signals() {
    menu_open_.signal_activate().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_open_directory));
    menu_check_all_.signal_activate().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_check_all));
    menu_domain_.signal_activate().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_change_domain));
    menu_quit_.signal_activate().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_quit));
    tool_open_.signal_clicked().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_open_directory));
    tool_check_.signal_clicked().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_check_all));
    signal_delete_event().connect(sigc::mem_fun(*this, &RunpodIcoClassifier::on_delete_event));
    check_finished_dispatcher_.connect(
        sigc::mem_fun(*this, &RunpodIcoClassifier::on_check_worker_finished));
}

void RunpodIcoClassifier::setup_accel_groups() {
    auto accel_group = Gtk::AccelGroup::create();
    add_accel_group(accel_group);
    menu_quit_.add_accelerator("activate", accel_group, GDK_KEY_q, Gdk::CONTROL_MASK,
                               Gtk::ACCEL_VISIBLE);
}

void RunpodIcoClassifier::setup_data() {
    set_status_text("Open an icon folder (File → Open Folder). Server: " +
                    config_.domain);
}

void RunpodIcoClassifier::on_startup_icon_directory() {
    if (!config_.icons_directory.empty() &&
        std::filesystem::is_directory(config_.icons_directory)) {
        load_directory(config_.icons_directory);
        return;
    }
    on_open_directory();
}

void RunpodIcoClassifier::on_open_directory() {
    std::string directory;
    if (!pick_icon_directory(*this, directory))
        return;
    load_directory(directory);
}

void RunpodIcoClassifier::on_check_all() {
    if (check_running_)
        return;
    if (treestore_icons_->children().empty()) {
        Gtk::MessageDialog dialog(*this, "Open an icon folder first.", false,
                                  Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK);
        dialog.run();
        return;
    }
    start_check_worker();
}

void RunpodIcoClassifier::on_change_domain() {
    DomainDialog dialog(*this, config_.domain);
    if (dialog.run() != Gtk::RESPONSE_OK)
        return;

    const auto new_domain = normalize_domain(dialog.domain());
    if (new_domain.empty())
        return;

    std::string error;
    if (!check_server_health(new_domain, error)) {
        Gtk::MessageDialog warn(
            *this, "Server unavailable:\n" + error, false, Gtk::MESSAGE_WARNING,
            Gtk::BUTTONS_OK);
        warn.run();
        return;
    }

    config_.domain = new_domain;
    if (!save_config(config_, error)) {
        Gtk::MessageDialog save_error(*this, "Failed to save settings:\n" + error,
                                      false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
        save_error.run();
    }
    set_status_text("Server address updated: " + config_.domain);
}

void RunpodIcoClassifier::on_quit() {
    close();
}

bool RunpodIcoClassifier::on_delete_event(GdkEventAny * /*event*/) {
    if (check_running_) {
        Gtk::MessageDialog dialog(*this, "Wait for the check to finish.", false,
                                  Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK);
        dialog.run();
        return true;
    }
    if (check_thread_.joinable())
        check_thread_.join();
    return false;
}

void RunpodIcoClassifier::load_directory(const std::string &directory) {
    std::string error;
    const auto files = list_icon_files(directory, error);
    if (!error.empty()) {
        Gtk::MessageDialog dialog(*this, error, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
        dialog.run();
        return;
    }

    treestore_icons_->clear();
    current_directory_ = directory;
    config_.icons_directory = directory;
    std::string save_error;
    save_config(config_, save_error);

    for (const auto &file : files) {
        const auto expected = parse_expected_from_filename(file.filename);
        auto row = *treestore_icons_->append();
        row[icon_cols_.is_file_row] = true;
        row[icon_cols_.filename] = file.filename;
        row[icon_cols_.filepath] = file.filepath;
        row[icon_cols_.expected] = format_expected_label(expected);
        row[icon_cols_.predicted] = "—";
        row[icon_cols_.confidence] = "—";
        row[icon_cols_.details] = "Awaiting check";
        set_row_pending(row);
    }

    set_status_text(Glib::ustring::compose("Folder: %1 (%2 files)", directory,
                                           files.size()));
}

void RunpodIcoClassifier::clear_results() {
    for (auto &child : treestore_icons_->children()) {
        auto row = *child;
        if (!row[icon_cols_.is_file_row])
            continue;
        clear_row_children(row);
        row[icon_cols_.predicted] = "—";
        row[icon_cols_.confidence] = "—";
        row[icon_cols_.details] = "Awaiting check";
        set_row_pending(row);
    }
}

void RunpodIcoClassifier::clear_row_children(const Gtk::TreeModel::Row &parent) {
    auto children = parent.children();
    while (!children.empty())
        treestore_icons_->erase(*children.begin());
}

void RunpodIcoClassifier::append_top3_children(const Gtk::TreeModel::Row &parent,
                                               const MatchResult &match) {
    int rank = 1;
    for (const auto &candidate : match.top3_base) {
        auto child = *treestore_icons_->append(parent.children());
        child[icon_cols_.is_file_row] = false;
        child[icon_cols_.filename] = Glib::ustring::compose("top-%1", rank);
        child[icon_cols_.predicted] = candidate.first;

        std::ostringstream conf;
        conf.setf(std::ios::fixed);
        conf.precision(1);
        conf << (candidate.second * 100.f) << '%';
        child[icon_cols_.confidence] = conf.str();
        child[icon_cols_.details] = "alternative base";
        ++rank;
    }

    const Gtk::TreeModel::Path path = treestore_icons_->get_path(parent);
    tree_view_.expand_row(path, false);
}

void RunpodIcoClassifier::set_row_pending(const Gtk::TreeModel::Row &row) {
    row[icon_cols_.status] = static_cast<int>(MatchVerdict::Pending);
    row[icon_cols_.status_pixbuf] = pixbuf_pending_;
}

Glib::RefPtr<Gdk::Pixbuf> RunpodIcoClassifier::pixbuf_for_verdict(
    MatchVerdict verdict) const {
    switch (verdict) {
    case MatchVerdict::Ok:
        return pixbuf_check_;
    case MatchVerdict::Warn:
        return pixbuf_warning_;
    case MatchVerdict::Fail:
        return pixbuf_cross_;
    default:
        return pixbuf_pending_;
    }
}

void RunpodIcoClassifier::apply_match_result(const Gtk::TreeModel::Row &row,
                                             const MatchResult &match) {
    if (!row[icon_cols_.is_file_row])
        return;

    clear_row_children(row);
    row[icon_cols_.status] = static_cast<int>(match.verdict);
    row[icon_cols_.status_pixbuf] = pixbuf_for_verdict(match.verdict);
    row[icon_cols_.predicted] = match.predicted_label;
    row[icon_cols_.confidence] = match.confidence_text;
    row[icon_cols_.details] = match.details;

    if (match.verdict != MatchVerdict::Ok && !match.top3_base.empty())
        append_top3_children(row, match);
}

void RunpodIcoClassifier::set_status_text(const Glib::ustring &text) {
    status_bar_.pop(status_context_id_);
    status_context_id_ = status_bar_.get_context_id("main");
    status_bar_.push(text, status_context_id_);
}

void RunpodIcoClassifier::set_check_sensitive(bool sensitive) {
    menu_check_all_.set_sensitive(sensitive);
    tool_check_.set_sensitive(sensitive);
    menu_open_.set_sensitive(sensitive);
    tool_open_.set_sensitive(sensitive);
}

void RunpodIcoClassifier::start_check_worker() {
    std::vector<InferRequestItem> requests;
    std::map<std::string, ExpectedName> expected_by_label;

    for (const auto &child : treestore_icons_->children()) {
        auto row = *child;
        if (!row[icon_cols_.is_file_row])
            continue;
        clear_row_children(row);
        InferRequestItem item;
        item.filepath = static_cast<Glib::ustring>(row[icon_cols_.filepath]);
        item.label = static_cast<Glib::ustring>(row[icon_cols_.filename]);
        expected_by_label[item.label] = parse_expected_from_filename(item.label);
        requests.push_back(std::move(item));
        row[icon_cols_.details] = "Sending to server...";
        row[icon_cols_.status] = static_cast<int>(MatchVerdict::Pending);
        row[icon_cols_.status_pixbuf] = pixbuf_pending_;
    }

    check_running_ = true;
    set_check_sensitive(false);
    set_status_text("Checking on API server...");

    const std::string domain = config_.domain;
    check_thread_ = std::thread([this, domain, requests = std::move(requests),
                                 expected_by_label = std::move(expected_by_label)]() mutable {
        worker_results_.clear();
        worker_error_.clear();

        constexpr std::size_t batch_size = 16;
        for (std::size_t offset = 0; offset < requests.size(); offset += batch_size) {
            const std::size_t end = std::min(offset + batch_size, requests.size());
            std::vector<InferRequestItem> batch(requests.begin() + static_cast<std::ptrdiff_t>(offset),
                                                requests.begin() + static_cast<std::ptrdiff_t>(end));

            std::vector<InferResponseItem> responses;
            std::string error;
            if (!infer_icons(domain, batch, responses, error)) {
                worker_error_ = error;
                break;
            }

            for (const auto &response : responses) {
                const auto it = expected_by_label.find(response.label);
                ExpectedName expected;
                if (it != expected_by_label.end())
                    expected = it->second;
                worker_results_.emplace_back(
                    response.label,
                    compare_expected_vs_prediction(expected, response.prediction));
            }
        }

        check_running_ = false;
        check_finished_dispatcher_.emit();
    });
}

void RunpodIcoClassifier::on_check_worker_finished() {
    if (check_thread_.joinable())
        check_thread_.join();

    if (!worker_error_.empty()) {
        Gtk::MessageDialog dialog(*this, "Check failed:\n" + worker_error_, false,
                                  Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
        dialog.run();
        clear_results();
        set_status_text("Check finished with an error");
        set_check_sensitive(true);
        return;
    }

    std::map<Glib::ustring, MatchResult> results_by_label;
    for (const auto &entry : worker_results_)
        results_by_label[Glib::ustring(entry.first)] = entry.second;

    int ok_count = 0;
    int warn_count = 0;
    int fail_count = 0;

    for (auto &child : treestore_icons_->children()) {
        auto row = *child;
        if (!row[icon_cols_.is_file_row])
            continue;
        const Glib::ustring label = row[icon_cols_.filename];
        const auto it = results_by_label.find(label);
        if (it == results_by_label.end()) {
            MatchResult missing;
            missing.verdict = MatchVerdict::Fail;
            missing.predicted_label = "—";
            missing.confidence_text = "—";
            missing.details = "No response from server";
            apply_match_result(row, missing);
            ++fail_count;
            continue;
        }

        apply_match_result(row, it->second);
        switch (it->second.verdict) {
        case MatchVerdict::Ok:
            ++ok_count;
            break;
        case MatchVerdict::Warn:
            ++warn_count;
            break;
        case MatchVerdict::Fail:
            ++fail_count;
            break;
        default:
            break;
        }
    }

    set_status_text(Glib::ustring::compose(
        "Done: %1 matches, %2 warnings, %3 failures", ok_count, warn_count,
        fail_count));
    set_check_sensitive(true);
}

int run(int argc, char *argv[]) {
    auto app = Gtk::Application::create(argc, argv, "snusoed.ico-classifier-gtk3");
    app->register_application();

    AppConfig config;
    std::string error;
    if (!load_config(config, error)) {
        Gtk::MessageDialog dialog("Failed to read settings:\n" + error, false,
                                  Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK);
        dialog.run();
        return 1;
    }

    if (!ensure_domain(config))
        return 0;

    RunpodIcoClassifier window(app, config);
    Glib::signal_idle().connect_once(
        sigc::mem_fun(window, &RunpodIcoClassifier::on_startup_icon_directory));
    return app->run(window);
}

} // namespace runpod_ico_classifier
