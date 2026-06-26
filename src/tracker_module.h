#ifndef TRACKER_MODULE_H
#define TRACKER__MODULE_H

void trk_module_post_load  (Trk* trk);
void trk_module_post_reload(Trk* trk);
void trk_module_update     (Trk* trk, Ring_Buffer* out);

#endif // TRACKER__MODULE_H
