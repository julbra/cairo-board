/*
 * configuration.h
 *
 *  Created on: 30 Nov 2009
 *      Author: hts
 */

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

void init_config(void);
gboolean save_config(void);
gchar *get_login(void);
void set_login(const gchar *login);
gchar *get_password(void);
void set_password(const gchar *login);
gboolean get_save_login(void);
void set_save_login(gboolean save);
gboolean get_auto_login(void);
void set_auto_login(gboolean autolog);


#endif /* CONFIGURATION_H_ */
