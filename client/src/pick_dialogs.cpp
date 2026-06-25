#include "runpod_ico_classifier/pick_dialogs.hpp"

#include <gtkmm/filechooserdialog.h>

namespace runpod_ico_classifier {

namespace {

void setup_domain_dialog_content(Gtk::Dialog &dialog, Gtk::Label &label_hint,
                                 Gtk::Entry &entry_domain,
                                 const std::string &initial_domain) {
    dialog.set_default_size(520, -1);
    dialog.set_border_width(12);

    auto *content = dialog.get_content_area();
    content->set_spacing(8);
    label_hint.set_text(
        "Enter the API server address. localhost supported as well");
    content->pack_start(label_hint, Gtk::PACK_SHRINK);
    entry_domain.set_text(initial_domain);
    entry_domain.set_hexpand(true);
    entry_domain.set_placeholder_text("http://127.0.0.1:8000");
    content->pack_start(entry_domain, Gtk::PACK_SHRINK);

    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Continue", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);
    dialog.show_all_children();
}

} // namespace

DomainDialog::DomainDialog(const std::string &initial_domain)
    : Gtk::Dialog("API Server Address", true), label_hint_(), entry_domain_() {
    setup_domain_dialog_content(*this, label_hint_, entry_domain_,
                                initial_domain);
}

DomainDialog::DomainDialog(Gtk::Window &parent,
                           const std::string &initial_domain)
    : Gtk::Dialog("API Server Address", parent, true), label_hint_(),
      entry_domain_() {
    setup_domain_dialog_content(*this, label_hint_, entry_domain_,
                                initial_domain);
}

std::string DomainDialog::domain() const { return entry_domain_.get_text(); }

bool pick_icon_directory(Gtk::Window &parent, std::string &directory_out) {
    Gtk::FileChooserDialog dialog("Select Icon Folder",
                                  Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
    dialog.set_transient_for(parent);
    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("Open", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);

    if (dialog.run() != Gtk::RESPONSE_OK)
        return false;

    directory_out = dialog.get_filename();
    return !directory_out.empty();
}

} // namespace runpod_ico_classifier
