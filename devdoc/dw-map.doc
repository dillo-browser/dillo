/** \page dw-map Dillo Widget Documentation Map

This maps includes special documentations as well as longer comments
in the sources. Arrows denote references between the documents.

\dot
digraph G {
   rankdir=LR;
   node [shape=record, fontname=Helvetica, fontsize=8];
   fontname=Helvetica; fontsize=8;

    dw_overview [label="Dillo Widget Overview", URL="\ref dw-overview"];
    dw_usage [label="Dillo Widget Usage", URL="\ref dw-usage"];
    dw_layout_views [label="Layout and Views", URL="\ref dw-layout-views"];
    dw_layout_widgets [label="Layout and Widgets",
                       URL="\ref dw-layout-widgets"];
    dw_widget_sizes [label="Sizes of Dillo Widgets",
                     URL="\ref dw-widget-sizes"];
    dw_changes [label="Changes to the GTK+-based Release Version",
                URL="\ref dw-changes"];
    dw_images_and_backgrounds [label="Images and Backgrounds in Dw",
                               URL="\ref dw-images-and-backgrounds"];
    dw_Image [label="dw::Image", URL="\ref dw::Image"];
    dw_core_Imgbuf [label="dw::core::Imgbuf", URL="\ref dw::core::Imgbuf"];
    dw_core_SelectionState [label="dw::core::SelectionState",
                              URL="\ref dw::core::SelectionState"];
    dw_core_style [label="dw::core::style", URL="\ref dw::core::style"];
    dw_Table [label="dw::Table", URL="\ref dw::Table"];
    dw_Textblock [label="dw::Textblock", URL="\ref dw::Textblock"];
    dw_core_ui [label="dw::core::ui", URL="\ref dw::core::ui"];

    dw_overview -> dw_changes;
    dw_overview -> dw_usage;
    dw_overview -> dw_core_style;
    dw_overview -> dw_core_ui;
    dw_overview -> dw_images_and_backgrounds;
    dw_overview -> dw_layout_widgets;
    dw_overview -> dw_widget_sizes;
    dw_overview -> dw_layout_views;

    dw_usage -> dw_Table;
    dw_usage -> dw_Textblock;
    dw_usage -> dw_core_style;
    dw_usage -> dw_core_ui;
    dw_usage -> dw_images_and_backgrounds;

    dw_layout_widgets -> dw_widget_sizes;
    dw_layout_widgets -> dw_core_SelectionState;

    dw_widget_sizes -> dw_Table;
    dw_widget_sizes -> dw_Textblock;

    dw_images_and_backgrounds -> dw_core_Imgbuf;
    dw_images_and_backgrounds -> dw_Image;

    dw_core_style -> dw_Textblock;
}
\enddot
*/