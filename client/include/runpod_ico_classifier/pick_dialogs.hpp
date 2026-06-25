#pragma once

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>

namespace runpod_ico_classifier {

class DomainDialog : public Gtk::Dialog {
public:
    explicit DomainDialog(const std::string &initial_domain);
    explicit DomainDialog(Gtk::Window &parent, const std::string &initial_domain);

    std::string domain() const;

private:
    Gtk::Label label_hint_;
    Gtk::Entry entry_domain_;
};

bool pick_icon_directory(Gtk::Window &parent, std::string &directory_out);

} // namespace runpod_ico_classifier
