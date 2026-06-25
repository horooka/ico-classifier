#pragma once

#include "runpod_ico_classifier/config_io.hpp"
#include "runpod_ico_classifier/icon_dir_io.hpp"
#include "runpod_ico_classifier/infer_client.hpp"
#include "runpod_ico_classifier/name_match.hpp"

#include <gtkmm.h>

#include <atomic>
#include <map>
#include <thread>
#include <vector>

namespace runpod_ico_classifier {

#define MIN_TREE_WIDTH 720
#define MIN_WINDOW_WIDTH 960
#define MIN_WINDOW_HEIGHT 560

class IconCols : public Gtk::TreeModel::ColumnRecord {
public:
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> status_pixbuf;
    Gtk::TreeModelColumn<int> status;
    Gtk::TreeModelColumn<Glib::ustring> filename;
    Gtk::TreeModelColumn<Glib::ustring> filepath;
    Gtk::TreeModelColumn<Glib::ustring> expected;
    Gtk::TreeModelColumn<Glib::ustring> predicted;
    Gtk::TreeModelColumn<Glib::ustring> confidence;
    Gtk::TreeModelColumn<Glib::ustring> details;
    Gtk::TreeModelColumn<bool> is_file_row;

    IconCols() {
        add(status_pixbuf);
        add(status);
        add(filename);
        add(filepath);
        add(expected);
        add(predicted);
        add(confidence);
        add(details);
        add(is_file_row);
    }
};

class RunpodIcoClassifier : public Gtk::ApplicationWindow {
public:
    RunpodIcoClassifier(const Glib::RefPtr<Gtk::Application> &app,
                        const AppConfig &config);

    void on_startup_icon_directory();

private:
    void setup_ui();
    void setup_menu_bar();
    void setup_toolbar();
    void setup_center_pane();
    void setup_status_bar();
    void setup_signals();
    void setup_accel_groups();
    void setup_data();
    void setup_stock_icons();

    void on_open_directory();
    void on_check_all();
    void on_change_domain();
    void on_quit();
    bool on_delete_event(GdkEventAny *event);

    void load_directory(const std::string &directory);
    void clear_results();
    void clear_row_children(const Gtk::TreeModel::Row &parent);
    void append_top3_children(const Gtk::TreeModel::Row &parent,
                              const MatchResult &match);
    void set_row_pending(const Gtk::TreeModel::Row &row);
    void apply_match_result(const Gtk::TreeModel::Row &row,
                            const MatchResult &match);
    Glib::RefPtr<Gdk::Pixbuf> pixbuf_for_verdict(MatchVerdict verdict) const;
    void set_status_text(const Glib::ustring &text);
    void set_check_sensitive(bool sensitive);
    void start_check_worker();
    void on_check_worker_finished();

    Glib::RefPtr<Gtk::Application> app_;
    AppConfig config_;

    Gtk::Box root_box_{Gtk::ORIENTATION_VERTICAL};
    Gtk::MenuBar menu_bar_;
    Gtk::MenuItem menu_file_;
    Gtk::MenuItem menu_open_;
    Gtk::MenuItem menu_quit_;
    Gtk::Menu menu_file_sub_;
    Gtk::MenuItem menu_service_;
    Gtk::MenuItem menu_domain_;
    Gtk::Menu menu_service_sub_;
    Gtk::MenuItem menu_check_;
    Gtk::MenuItem menu_check_all_;
    Gtk::Menu menu_check_sub_;

    Gtk::Toolbar toolbar_;
    Gtk::ToolButton tool_open_;
    Gtk::SeparatorToolItem toolbar_sep_;
    Gtk::ToolButton tool_check_;

    Gtk::ScrolledWindow tree_scroller_;
    Gtk::TreeView tree_view_;
    IconCols icon_cols_;
    Glib::RefPtr<Gtk::TreeStore> treestore_icons_;

    Gtk::Statusbar status_bar_;
    guint status_context_id_ = 0;

    Glib::RefPtr<Gdk::Pixbuf> pixbuf_check_;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf_cross_;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf_warning_;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf_pending_;

    std::string current_directory_;
    std::atomic<bool> check_running_{false};
    std::thread check_thread_;
    Glib::Dispatcher check_finished_dispatcher_;
    std::string worker_error_;
    std::vector<std::pair<std::string, MatchResult>> worker_results_;
};

int run(int argc, char *argv[]);

} // namespace runpod_ico_classifier
