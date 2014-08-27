#include <stdlib.h>
#include <string.h>
#include <roxml_mem.h>
#include <roxml_core.h>
#include <roxml_xpath.h>
#include <roxml_parser.h>

ROXML_INT int roxml_is_number(char *input)
{        
	char *end;                  
	int is_number = 0;        

	/*
	 * we don't need the value per se and some compiler will
	 * complain about an initialized but unused variable if we
	 * get it.                                    
	 */                           
	strtod(input, &end);

	if ((end == NULL) || (roxml_is_separator(end[0])) || (end[0] == '"') || (end[0] == '\'') || (end[0] == '\0')) {
		is_number = 1;
	}                              

	return is_number; 
}

ROXML_API node_t **roxml_xpath(node_t *n, char *path, int *nb_ans)
{
	int count = 0;
	node_t **node_set = NULL;
	int index = 0;
	xpath_node_t *xpath = NULL;
	node_t *root = n;
	char *full_path_to_find;

	if (n == NULL) {
		if (nb_ans) {
			*nb_ans = 0;
		}
		return NULL;
	}

	root = roxml_get_root(n);

	full_path_to_find = strdup(path);

	index = roxml_parse_xpath(full_path_to_find, &xpath, 0);

	if (index >= 0) {
		node_set = roxml_exec_xpath(root, n, xpath, index, &count);

		roxml_free_xpath(xpath, index);
		free(full_path_to_find);

		if (count == 0) {
			roxml_release(node_set);
			node_set = NULL;
		}
	}
	if (nb_ans) {
		*nb_ans = count;
	}

	return node_set;
}

ROXML_INT xpath_node_t *roxml_set_axes(xpath_node_t *node, char *axes, int *offset)
{
	struct _xpath_axes {
		char id;
		char *name;
	};

	struct _xpath_axes xpath_axes[14] = {
		{ROXML_ID_PARENT, ROXML_L_PARENT},
		{ROXML_ID_PARENT, ROXML_S_PARENT},
		{ROXML_ID_SELF, ROXML_L_SELF},
		{ROXML_ID_SELF, ROXML_S_SELF},
		{ROXML_ID_ATTR, ROXML_L_ATTR},
		{ROXML_ID_ATTR, ROXML_S_ATTR},
		{ROXML_ID_ANC, ROXML_L_ANC},
		{ROXML_ID_ANC_O_SELF, ROXML_L_ANC_O_SELF},
		{ROXML_ID_NEXT_SIBL, ROXML_L_NEXT_SIBL},
		{ROXML_ID_PREV_SIBL, ROXML_L_PREV_SIBL},
		{ROXML_ID_NEXT, ROXML_L_NEXT},
		{ROXML_ID_PREV, ROXML_L_PREV},
		{ROXML_ID_NS, ROXML_L_NS},
		{ROXML_ID_CHILD, ROXML_L_CHILD},
	};

	xpath_node_t *tmp_node;
	if (axes[0] == '/') {
		axes[0] = '\0';
		*offset += 1;
		axes++;
	}
	if (axes[0] == '/') {
		// ROXML_S_DESC_O_SELF
		node->axes = ROXML_ID_DESC_O_SELF;
		node->name = axes + 1;
		tmp_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
		tmp_node->axes = ROXML_ID_CHILD;
		node->next = tmp_node;
		if (strlen(node->name) > 0) {
			tmp_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
			node->next->next = tmp_node;
			node = roxml_set_axes(tmp_node, axes + 1, offset);
		}
	} else if (strncmp(ROXML_L_DESC_O_SELF, axes, strlen(ROXML_L_DESC_O_SELF)) == 0) {
		// ROXML_L_DESC_O_SELF
		node->axes = ROXML_ID_DESC_O_SELF;
		node->name = axes + strlen(ROXML_L_DESC_O_SELF);
		*offset += strlen(ROXML_L_DESC_O_SELF);
		tmp_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
		tmp_node->axes = ROXML_ID_CHILD;
		node->next = tmp_node;
		node = roxml_set_axes(tmp_node, axes + strlen(ROXML_L_DESC_O_SELF), offset);
	} else if (strncmp(ROXML_L_DESC, axes, strlen(ROXML_L_DESC)) == 0) {
		// ROXML_L_DESC
		node->axes = ROXML_ID_DESC;
		node->name = axes + strlen(ROXML_L_DESC);
		*offset += strlen(ROXML_L_DESC);
		tmp_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
		tmp_node->axes = ROXML_ID_CHILD;
		node->next = tmp_node;
		node = roxml_set_axes(tmp_node, axes + strlen(ROXML_L_DESC), offset);
	} else {
		int i = 0;

		// ROXML_S_CHILD is default
		node->axes = ROXML_ID_CHILD;
		node->name = axes;

		for (i = 0; i < 14; i++) {
			int len = strlen(xpath_axes[i].name);
			if (strncmp(xpath_axes[i].name, axes, len) == 0) {
				node->axes = xpath_axes[i].id;
				node->name = axes + len;
				break;
			}
		}
	}
	return node;
}

ROXML_INT int roxml_get_node_internal_position(node_t *n)
{
	int idx = 1;
	node_t *prnt;
	node_t *first;
	if (n == NULL) {
		return 0;
	}

	prnt = n->prnt;
	if (!prnt) {
		return 1;
	}
	first = prnt->chld;

	while ((first) && (first != n)) {
		idx++;
		first = first->sibl;
	}

	return idx;
}

ROXML_INT int roxml_parse_xpath(char *path, xpath_node_t **xpath, int context)
{
	int ret = 0;
	roxml_xpath_ctx_t ctx;
	roxml_parser_item_t *parser = NULL;
	ctx.pos = 0;
	ctx.nbpath = 1;
	ctx.bracket = 0;
	ctx.parenthesys = 0;
	ctx.quoted = 0;
	ctx.dquoted = 0;
	ctx.content_quoted = 0;
	ctx.is_first_node = 1;
	ctx.wait_first_node = 1;
	ctx.shorten_cond = 0;
	ctx.context = context;
	ctx.first_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
	ctx.new_node = ctx.first_node;
	ctx.new_cond = NULL;
	ctx.first_node->rel = ROXML_OPERATOR_OR;

	parser = roxml_append_parser_item(parser, " ", _func_xpath_ignore);
	parser = roxml_append_parser_item(parser, "\t", _func_xpath_ignore);
	parser = roxml_append_parser_item(parser, "\n", _func_xpath_ignore);
	parser = roxml_append_parser_item(parser, "\r", _func_xpath_ignore);
	parser = roxml_append_parser_item(parser, "\"", _func_xpath_dquote);
	parser = roxml_append_parser_item(parser, "\'", _func_xpath_quote);
	parser = roxml_append_parser_item(parser, "/", _func_xpath_new_node);
	parser = roxml_append_parser_item(parser, "(", _func_xpath_open_parenthesys);
	parser = roxml_append_parser_item(parser, ")", _func_xpath_close_parenthesys);
	parser = roxml_append_parser_item(parser, "[", _func_xpath_open_brackets);
	parser = roxml_append_parser_item(parser, "]", _func_xpath_close_brackets);
	parser = roxml_append_parser_item(parser, "=", _func_xpath_operator_equal);
	parser = roxml_append_parser_item(parser, ">", _func_xpath_operator_sup);
	parser = roxml_append_parser_item(parser, "<", _func_xpath_operator_inf);
	parser = roxml_append_parser_item(parser, "!", _func_xpath_operator_diff);
	parser = roxml_append_parser_item(parser, "0", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "1", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "2", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "3", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "4", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "5", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "6", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "7", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "8", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "9", _func_xpath_number);
	parser = roxml_append_parser_item(parser, "+", _func_xpath_operator_add);
	parser = roxml_append_parser_item(parser, "-", _func_xpath_operator_subs);
	parser = roxml_append_parser_item(parser, ROXML_PATH_OR, _func_xpath_path_or);
	parser = roxml_append_parser_item(parser, ROXML_COND_OR, _func_xpath_condition_or);
	parser = roxml_append_parser_item(parser, ROXML_COND_AND, _func_xpath_condition_and);
	parser = roxml_append_parser_item(parser, ROXML_FUNC_POS_STR, _func_xpath_position);
	parser = roxml_append_parser_item(parser, ROXML_FUNC_FIRST_STR, _func_xpath_first);
	parser = roxml_append_parser_item(parser, ROXML_FUNC_LAST_STR, _func_xpath_last);
	parser = roxml_append_parser_item(parser, ROXML_FUNC_NSURI_STR, _func_xpath_nsuri);
	parser = roxml_append_parser_item(parser, ROXML_FUNC_LNAME_STR, _func_xpath_lname);
	parser = roxml_append_parser_item(parser, NULL, _func_xpath_default);

	parser = roxml_parser_prepare(parser);
	ret = roxml_parse_line(parser, path, 0, &ctx);
	roxml_parser_free(parser);

	if (ret >= 0) {
		if (xpath) {
			*xpath = ctx.first_node;
		}
		return ctx.nbpath;
	}

	roxml_free_xpath(ctx.first_node, ctx.nbpath);
	return -1;
}

ROXML_INT void roxml_free_xcond(xpath_cond_t * xcond)
{
	if (xcond->next) {
		roxml_free_xcond(xcond->next);
	}
	if (xcond->xp) {
		roxml_free_xpath(xcond->xp, xcond->func2);
	}
	free(xcond);
}

ROXML_INT void roxml_free_xpath(xpath_node_t *xpath, int nb)
{
	int i = 0;
	for (i = 0; i < nb; i++) {
		if (xpath[i].next) {
			roxml_free_xpath(xpath[i].next, 1);
		}
		if (xpath[i].cond) {
			roxml_free_xcond(xpath[i].cond);
		}
		free(xpath[i].xp_cond);
	}
	free(xpath);
}

ROXML_INT double roxml_double_oper(double a, double b, int op)
{
	if (op == ROXML_OPERATOR_ADD) {
		return (a + b);
	} else if (op == ROXML_OPERATOR_SUB) {
		return (a - b);
	} else if (op == ROXML_OPERATOR_MUL) {
		return (a * b);
	} else if (op == ROXML_OPERATOR_DIV) {
		return (a / b);
	}
	return 0;
}

ROXML_INT int roxml_double_cmp(double a, double b, int op)
{
	if (op == ROXML_OPERATOR_DIFF) {
		return (a != b);
	} else if (op == ROXML_OPERATOR_EINF) {
		return (a <= b);
	} else if (op == ROXML_OPERATOR_INF) {
		return (a < b);
	} else if (op == ROXML_OPERATOR_ESUP) {
		return (a >= b);
	} else if (op == ROXML_OPERATOR_SUP) {
		return (a > b);
	} else if (op == ROXML_OPERATOR_EQU) {
		return (a == b);
	}
	return 0;
}

ROXML_INT int roxml_string_cmp(char *sa, char *sb, int op)
{
	int result;

	result = strcmp(sa, sb);

	if (op == ROXML_OPERATOR_DIFF) {
		return (result != 0);
	} else if (op == ROXML_OPERATOR_EINF) {
		return (result <= 0);
	} else if (op == ROXML_OPERATOR_INF) {
		return (result < 0);
	} else if (op == ROXML_OPERATOR_ESUP) {
		return (result >= 0);
	} else if (op == ROXML_OPERATOR_SUP) {
		return (result > 0);
	} else if (op == ROXML_OPERATOR_EQU) {
		return (result == 0);
	}
	return 0;
}

ROXML_INT int roxml_validate_predicat(xpath_node_t *xn, node_t * candidat)
{
	int first = 1;
	int valid = 0;
	xpath_cond_t *condition;

	if (xn == NULL) {
		return 1;
	}

	condition = xn->cond;

	if (!condition) {
		return 1;
	}

	while (condition) {
		int status = 0;
		double iarg1;
		double iarg2;
		char *sarg1;
		char *sarg2;

		if (condition->func == ROXML_FUNC_POS) {
			status = 0;
			iarg2 = atof(condition->arg2);
			if (xn->name[0] == '*') {
				iarg1 = roxml_get_node_internal_position(candidat);
			} else {
				iarg1 = roxml_get_node_position(candidat);
			}
			status = roxml_double_cmp(iarg1, iarg2, condition->op);
		} else if (condition->func == ROXML_FUNC_LAST) {
			status = 0;
			iarg2 = roxml_get_chld_nb(candidat->prnt);
			if (xn->name[0] == '*') {
				iarg1 = roxml_get_node_internal_position(candidat);
			} else {
				iarg1 = roxml_get_node_position(candidat);
			}
			if (condition->op > 0) {
				double operand = 0;
				operand = atof(condition->arg2);
				iarg2 = roxml_double_oper(iarg2, operand, condition->op);
			}
			status = roxml_double_cmp(iarg1, iarg2, ROXML_OPERATOR_EQU);
		} else if (condition->func == ROXML_FUNC_FIRST) {
			status = 0;
			iarg2 = 1;
			if (xn->name[0] == '*') {
				iarg1 = roxml_get_node_internal_position(candidat);
			} else {
				iarg1 = roxml_get_node_position(candidat);
			}
			if (condition->op > 0) {
				double operand = 0;
				operand = atof(condition->arg2);
				iarg2 = roxml_double_oper(iarg2, operand, condition->op);
			}
			status = roxml_double_cmp(iarg1, iarg2, ROXML_OPERATOR_EQU);
		} else if (condition->func == ROXML_FUNC_INTCOMP) {
			node_t *val = roxml_get_attr(candidat, condition->arg1 + 1, 0);
			status = 0;
			if (val) {
				ROXML_GET_BASE_BUFFER(strval);
				iarg1 = atof(roxml_get_content(val, strval, ROXML_BASE_LEN, &status));
				if (status >= ROXML_BASE_LEN) {
					iarg1 = atof(roxml_get_content(val, NULL, 0, &status));
					roxml_release(RELEASE_LAST);
				}
				iarg2 = atof(condition->arg2);
				status = roxml_double_cmp(iarg1, iarg2, condition->op);
				ROXML_PUT_BASE_BUFFER(strval);
			}
		} else if (condition->func == ROXML_FUNC_NSURI) {
			node_t *val = roxml_get_ns(candidat);
			status = 0;
			if (val) {
				ROXML_GET_BASE_BUFFER(strval);
				sarg1 = roxml_get_content(val, strval, ROXML_BASE_LEN, &status);
				if (status >= ROXML_BASE_LEN) {
					sarg1 = roxml_get_content(val, NULL, 0, &status);
				}
				sarg2 = condition->arg2;
				status = roxml_string_cmp(sarg1, sarg2, condition->op);
				roxml_release(sarg1);
				ROXML_PUT_BASE_BUFFER(strval);
			} else {
				sarg2 = condition->arg2;
				status = roxml_string_cmp("", sarg2, condition->op);
			}
		} else if (condition->func == ROXML_FUNC_LNAME) {
			ROXML_GET_BASE_BUFFER(strval);
			roxml_get_name(candidat, strval, ROXML_BASE_LEN);
			status = strcmp(strval, condition->arg2) == 0;
			ROXML_PUT_BASE_BUFFER(strval);
		} else if (condition->func == ROXML_FUNC_STRCOMP) {
			node_t *val = roxml_get_attr(candidat, condition->arg1 + 1, 0);
			status = 0;
			if (val) {
				ROXML_GET_BASE_BUFFER(strval);
				sarg1 = roxml_get_content(val, strval, ROXML_BASE_LEN, &status);
				if (status >= ROXML_BASE_LEN) {
					sarg1 = roxml_get_content(val, NULL, 0, &status);
				}
				sarg2 = condition->arg2;
				status = roxml_string_cmp(sarg1, sarg2, condition->op);
				roxml_release(sarg1);
				ROXML_PUT_BASE_BUFFER(strval);
			}
		} else if (condition->func == ROXML_FUNC_XPATH) {
			int index = condition->func2;
			int count = 0;
			node_t *root = roxml_get_root(candidat);
			node_t **node_set;
			status = 0;

			node_set = roxml_exec_xpath(root, candidat, condition->xp, index, &count);

			roxml_release(node_set);

			if (count > 0) {
				status = 1;
			}

		}

		if (first) {
			valid = status;
			first = 0;
		} else {
			if (condition->rel == ROXML_OPERATOR_OR) {
				valid = valid || status;
			} else if (condition->rel == ROXML_OPERATOR_AND) {
				valid = valid && status;
			}
		}
		condition = condition->next;
	}

	return valid;
}

ROXML_INT int roxml_request_id(node_t *root)
{
	int i = 0;
	xpath_tok_table_t *table;

	while (root->prnt) {
		root = root->prnt;
	}

	table = (xpath_tok_table_t *) root->priv;

	pthread_mutex_lock(&table->mut);
	for (i = ROXML_XPATH_FIRST_ID; i < 255; i++) {
		if (table->ids[i] == 0) {
			table->ids[i]++;
			pthread_mutex_unlock(&table->mut);
			return i;
		}
	}
	pthread_mutex_unlock(&table->mut);
	return -1;
}

ROXML_INT int roxml_in_pool(node_t *root, node_t * n, int req_id)
{
	xpath_tok_table_t *table;

	while (root->prnt) {
		root = root->prnt;
	}

	table = (xpath_tok_table_t *) root->priv;

	pthread_mutex_lock(&table->mut);
	if (n->priv) {
		xpath_tok_t *tok = (xpath_tok_t *) n->priv;
		if (tok->id == req_id) {
			pthread_mutex_unlock(&table->mut);
			return 1;
		} else {
			while (tok) {
				if (tok->id == req_id) {
					pthread_mutex_unlock(&table->mut);
					return 1;
				}
				tok = tok->next;
			}
		}
	}
	pthread_mutex_unlock(&table->mut);
	return 0;
}

ROXML_INT void roxml_release_id(node_t *root, node_t **pool, int pool_len, int req_id)
{
	int i = 0;
	xpath_tok_table_t *table = NULL;

	while (root->prnt) {
		root = root->prnt;
	}

	table = (xpath_tok_table_t *) root->priv;

	for (i = 0; i < pool_len; i++) {
		roxml_del_from_pool(root, pool[i], req_id);
	}
	pthread_mutex_lock(&table->mut);
	table->ids[req_id] = 0;
	pthread_mutex_unlock(&table->mut);
}

ROXML_INT void roxml_del_from_pool(node_t *root, node_t * n, int req_id)
{
	xpath_tok_table_t *table = NULL;

	while (root->prnt) {
		root = root->prnt;
	}

	table = (xpath_tok_table_t *) root->priv;

	pthread_mutex_lock(&table->mut);
	if (n->priv) {
		xpath_tok_t *prev = (xpath_tok_t *) n->priv;
		xpath_tok_t *tok = (xpath_tok_t *) n->priv;
		if (tok->id == req_id) {
			n->priv = (void *)tok->next;
			free(tok);
		} else {
			while (tok) {
				if (tok->id == req_id) {
					prev->next = tok->next;
					free(tok);
					break;
				}
				prev = tok;
				tok = tok->next;
			}
		}
	}
	pthread_mutex_unlock(&table->mut);
}

ROXML_INT int roxml_add_to_pool(node_t *root, node_t * n, int req_id)
{
	xpath_tok_table_t *table;
	xpath_tok_t *tok;
	xpath_tok_t *last_tok = NULL;

	while (root->prnt) {
		root = root->prnt;
	}

	if (req_id == 0) {
		return 1;
	}
	table = (xpath_tok_table_t *) root->priv;

	pthread_mutex_lock(&table->mut);
	tok = (xpath_tok_t *) n->priv;

	while (tok) {
		if (tok->id == req_id) {
			pthread_mutex_unlock(&table->mut);
			return 0;
		}
		last_tok = tok;
		tok = (xpath_tok_t *) tok->next;
	}
	if (last_tok == NULL) {
		n->priv = calloc(1, sizeof(xpath_tok_t));
		last_tok = (xpath_tok_t *) n->priv;
	} else {
		last_tok->next = (xpath_tok_t *) calloc(1, sizeof(xpath_tok_t));
		last_tok = last_tok->next;
	}
	last_tok->id = req_id;
	pthread_mutex_unlock(&table->mut);

	return 1;
}

ROXML_INT int roxml_validate_axes(node_t *root, node_t * candidat, node_t *** ans, int *nb, int *max, xpath_node_t *xn, int req_id)
{

	int valid = 0;
	int path_end = 0;
	char *axes = NULL;

	if (xn == NULL) {
		valid = 1;
		path_end = 1;
	} else {
		axes = xn->name;

		if ((axes == NULL) || (strcmp("node()", axes) == 0)) {
			valid = 1;
		} else if (strcmp("*", axes) == 0) {
			if (candidat->type & ROXML_ELM_NODE) {
				valid = 1;
			}
			if (candidat->type & ROXML_ATTR_NODE) {
				valid = 1;
			}
		} else if (strcmp("comment()", axes) == 0) {
			if (candidat->type & ROXML_CMT_NODE) {
				valid = 1;
			}
		} else if (strcmp("processing-instruction()", axes) == 0) {
			if (candidat->type & ROXML_PI_NODE) {
				valid = 1;
			}
		} else if (strcmp("text()", axes) == 0) {
			if (candidat->type & ROXML_TXT_NODE) {
				valid = 1;
			}
		} else if (strcmp("", axes) == 0) {
			if (xn->abs) {
				candidat = root;
				valid = 1;
			}
		}
		if (!valid) {
			if (candidat->type & ROXML_PI_NODE) {
				return 0;
			}
			if (candidat->type & ROXML_CMT_NODE) {
				return 0;
			}
		}
		if (xn->next == NULL) {
			path_end = 1;
		}
		if ((xn->axes == ROXML_ID_SELF) || (xn->axes == ROXML_ID_PARENT)) {
			valid = 1;
		}
	}

	if (!valid) {
		ROXML_GET_BASE_BUFFER(intern_buff);
		int ns_len = 0;
		char *name = intern_buff;
		if (candidat->ns) {
			name = roxml_get_name(candidat->ns, intern_buff, ROXML_BASE_LEN);
			ns_len = strlen(name);
			if (ns_len) {
				name[ns_len] = ':';
				ns_len++;
			}
		}
		roxml_get_name(candidat, intern_buff + ns_len, ROXML_BASE_LEN - ns_len);
		if (name && strcmp(name, axes) == 0) {
			valid = 1;
		}
		ROXML_PUT_BASE_BUFFER(intern_buff);
	}

	if (valid) {
		valid = roxml_validate_predicat(xn, candidat);
	}

	if ((valid) && (xn) && (xn->xp_cond)) {
		int status;
		xpath_cond_t *condition = xn->xp_cond;
		valid = 0;
		if (condition->func == ROXML_FUNC_STRCOMP) {
			char *sarg1;
			char *sarg2;
			ROXML_GET_BASE_BUFFER(strval);
			sarg1 = roxml_get_content(candidat, strval, ROXML_BASE_LEN, &status);
			if (status >= ROXML_BASE_LEN) {
				sarg1 = roxml_get_content(candidat, NULL, 0, &status);
			}
			sarg2 = condition->arg2;
			valid = roxml_string_cmp(sarg1, sarg2, condition->op);
			roxml_release(sarg1);
			ROXML_PUT_BASE_BUFFER(strval);
		} else if (condition->func == ROXML_FUNC_INTCOMP) {
			double iarg1;
			double iarg2;
			ROXML_GET_BASE_BUFFER(strval);
			iarg1 = atof(roxml_get_content(candidat, strval, ROXML_BASE_LEN, &status));
			if (status >= ROXML_BASE_LEN) {
				iarg1 = atof(roxml_get_content(candidat, NULL, 0, &status));
				roxml_release(RELEASE_LAST);
			}
			iarg2 = atof(condition->arg2);
			valid = roxml_double_cmp(iarg1, iarg2, condition->op);
			ROXML_PUT_BASE_BUFFER(strval);
		}
	}

	if ((valid) && (path_end)) {
		if (roxml_add_to_pool(root, candidat, req_id)) {
			if (ans) {
				if ((*nb) >= (*max)) {
					int new_max = (*max) * 2;
					node_t **new_ans = roxml_malloc(sizeof(node_t *), new_max, PTR_NODE_RESULT);
					memcpy(new_ans, (*ans), *(max) * sizeof(node_t *));
					roxml_release(*ans);
					*ans = new_ans;
					*max = new_max;
				}
				(*ans)[*nb] = candidat;
			}
			(*nb)++;
		}
	}

	return valid;
}

ROXML_INT void roxml_check_node(xpath_node_t *xp, node_t * root, node_t * context, node_t *** ans, int *nb, int *max, int ignore, int req_id)
{
	int validate_node = 0;

	if ((req_id == 0) && (*nb > 0)) {
		return;
	}

	if (!xp) {
		return;
	}
	// if found a "all document" axes
	if (ignore == ROXML_DESC_ONLY) {
		node_t *current = context->chld;
		while (current) {
			roxml_check_node(xp, root, current, ans, nb, max, ignore, req_id);
			current = current->sibl;
		}
	}

	switch (xp->axes) {
	case ROXML_ID_CHILD:{
			node_t *current = context->chld;
			while (current) {
				validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
				if (validate_node) {
					roxml_check_node(xp->next, root, current, ans, nb, max, ROXML_DIRECT, req_id);
				}
				current = current->sibl;
			}
			if ((xp->name == NULL) || (strcmp(xp->name, "text()") == 0)
			    || (strcmp(xp->name, "node()") == 0)) {
				node_t *current = roxml_get_txt(context, 0);
				while (current) {
					validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
					current = current->sibl;
				}
			}
			if ((xp->name == NULL) || (strcmp(xp->name, "node()") == 0)) {
				node_t *current = context->attr;
				while (current) {
					validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
					current = current->sibl;
				}
			}
		}
		break;
	case ROXML_ID_DESC:{
			xp = xp->next;
			roxml_check_node(xp, root, context, ans, nb, max, ROXML_DESC_ONLY, req_id);
		}
		break;
	case ROXML_ID_DESC_O_SELF:{
			xp = xp->next;
			validate_node = roxml_validate_axes(root, context, ans, nb, max, xp, req_id);
			if (validate_node) {
				roxml_check_node(xp->next, root, context, ans, nb, max, ROXML_DIRECT, req_id);
			}
			roxml_check_node(xp, root, context, ans, nb, max, ROXML_DESC_ONLY, req_id);
		}
		break;
	case ROXML_ID_SELF:{
			validate_node = roxml_validate_axes(root, context, ans, nb, max, xp, req_id);
			roxml_check_node(xp->next, root, context, ans, nb, max, ROXML_DIRECT, req_id);
		}
		break;
	case ROXML_ID_PARENT:{
			if (context->prnt) {
				validate_node = roxml_validate_axes(root, context->prnt, ans, nb, max, xp, req_id);
				roxml_check_node(xp->next, root, context->prnt, ans, nb, max, ROXML_DIRECT, req_id);
			} else {
				validate_node = 0;
			}
		}
		break;
	case ROXML_ID_ATTR:{
			node_t *attribute = context->attr;
			while (attribute) {
				validate_node = roxml_validate_axes(root, attribute, ans, nb, max, xp, req_id);
				if (validate_node) {
					roxml_check_node(xp->next, root, context, ans, nb, max, ROXML_DIRECT, req_id);
				}
				attribute = attribute->sibl;
			}
		}
		break;
	case ROXML_ID_ANC:{
			node_t *current = context->prnt;
			while (current) {
				validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
				if (validate_node) {
					roxml_check_node(xp->next, root, current, ans, nb, max, ROXML_DIRECT, req_id);
				}
				current = current->prnt;
			}
		}
		break;
	case ROXML_ID_NEXT_SIBL:{
			node_t *current = context->sibl;
			while (current) {
				validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
				if (validate_node) {
					roxml_check_node(xp->next, root, current, ans, nb, max, ROXML_DIRECT, req_id);
				}
				current = current->sibl;
			}
		}
		break;
	case ROXML_ID_PREV_SIBL:{
			node_t *current = context->prnt->chld;
			while (current != context) {
				validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
				if (validate_node) {
					roxml_check_node(xp->next, root, current, ans, nb, max, ROXML_DIRECT, req_id);
				}
				current = current->sibl;
			}
		}
		break;
	case ROXML_ID_NEXT:{
			node_t *current = context;
			while (current) {
				node_t *following = current->sibl;
				while (following) {
					validate_node = roxml_validate_axes(root, following, ans, nb, max, xp, req_id);
					if (validate_node) {
						roxml_check_node(xp->next, root, following, ans, nb, max, ROXML_DIRECT,
								 req_id);
					} else {
						xp->axes = ROXML_ID_CHILD;
						roxml_check_node(xp, root, following, ans, nb, max, ROXML_DESC_ONLY,
								 req_id);
						xp->axes = ROXML_ID_NEXT;
					}
					following = following->sibl;
				}
				following = current->prnt->chld;
				while (following != current) {
					following = following->sibl;
				}
				current = following->sibl;
			}
		}
		break;
	case ROXML_ID_PREV:{
			node_t *current = context;
			while (current && current->prnt) {
				node_t *preceding = current->prnt->chld;
				while (preceding != current) {
					validate_node = roxml_validate_axes(root, preceding, ans, nb, max, xp, req_id);
					if (validate_node) {
						roxml_check_node(xp->next, root, preceding, ans, nb, max, ROXML_DIRECT,
								 req_id);
					} else {
						xp->axes = ROXML_ID_CHILD;
						roxml_check_node(xp, root, preceding, ans, nb, max, ROXML_DESC_ONLY,
								 req_id);
						xp->axes = ROXML_ID_PREV;
					}
					preceding = preceding->sibl;
				}
				current = current->prnt;
			}
		}
		break;
	case ROXML_ID_NS:{
			validate_node = roxml_validate_axes(root, context->ns, ans, nb, max, xp, req_id);
			if (validate_node) {
				roxml_check_node(xp->next, root, context, ans, nb, max, ROXML_DIRECT, req_id);
			}
		}
		break;
	case ROXML_ID_ANC_O_SELF:{
			node_t *current = context;
			while (current) {
				validate_node = roxml_validate_axes(root, current, ans, nb, max, xp, req_id);
				if (validate_node) {
					roxml_check_node(xp->next, root, current, ans, nb, max, ROXML_DIRECT, req_id);
				}
				current = current->prnt;
			}
		}
		break;
	}

	return;
}

ROXML_INT void roxml_compute_and(node_t *root, node_t **node_set, int *count, int cur_req_id, int prev_req_id)
{
	int i = 0;
	int limit = *count;
	int pool1 = 0, pool2 = 0;

	for (i = 0; i < limit; i++) {
		if (pool1 == 0)
			if (roxml_in_pool(root, node_set[i], cur_req_id))
				pool1++;
		if (pool2 == 0)
			if (roxml_in_pool(root, node_set[i], prev_req_id))
				pool2++;
		if (pool1 && pool2)
			break;
	}

	if (!pool1 || !pool2) {
		for (i = 0; i < limit; i++) {
			roxml_del_from_pool(root, node_set[i], cur_req_id);
			roxml_del_from_pool(root, node_set[i], prev_req_id);
		}
		*count = 0;
	}
}

ROXML_INT void roxml_compute_or(node_t *root, node_t **node_set, int *count, int req_id, int glob_id)
{
	int i = 0;
	for (i = 0; i < *count; i++) {
		if (roxml_in_pool(root, node_set[i], req_id)) {
			roxml_add_to_pool(root, node_set[i], glob_id);
			roxml_del_from_pool(root, node_set[i], req_id);
		}
	}
}

ROXML_INT node_t **roxml_exec_xpath(node_t *root, node_t * n, xpath_node_t * xpath, int index, int *count)
{
	int path_id;
	int max_answers = 1;
	int glob_id = 0;
	int *req_ids = NULL;

	node_t **node_set = roxml_malloc(sizeof(node_t *), max_answers, PTR_NODE_RESULT);

	*count = 0;
	glob_id = roxml_request_id(root);
	if (glob_id < 0) {
		roxml_release(node_set);
		return NULL;
	}
	req_ids = calloc(index, sizeof(int));

	// process all and xpath
	for (path_id = 0; path_id < index; path_id++) {
		xpath_node_t *cur_xpath = NULL;
		xpath_node_t *next_xpath = NULL;
		cur_xpath = &xpath[path_id];
		if (path_id < index - 1) {
			next_xpath = &xpath[path_id + 1];
		}

		if ((cur_xpath->rel == ROXML_OPERATOR_AND) || ((next_xpath) && (next_xpath->rel == ROXML_OPERATOR_AND))) {
			int req_id = roxml_request_id(root);

			node_t *orig = n;
			if (cur_xpath->abs) {
				// context node is root
				orig = root;
			}
			// assign a new request ID
			roxml_check_node(cur_xpath, root, orig, &node_set, count, &max_answers, ROXML_DIRECT, req_id);

			if (cur_xpath->rel == ROXML_OPERATOR_AND) {
				roxml_compute_and(root, node_set, count, req_id, req_ids[path_id - 1]);
			}
			req_ids[path_id] = req_id;
		}
	}

	// process all or xpath
	for (path_id = 0; path_id < index; path_id++) {
		node_t *orig = n;
		xpath_node_t *cur_xpath = &xpath[path_id];

		if (cur_xpath->rel == ROXML_OPERATOR_OR) {
			if (req_ids[path_id] == 0) {
				if (cur_xpath->abs) {
					// context node is root
					orig = root;
				}
				// assign a new request ID
				roxml_check_node(cur_xpath, root, orig, &node_set, count, &max_answers, ROXML_DIRECT,
						 glob_id);
			} else {
				roxml_compute_or(root, node_set, count, req_ids[path_id + 1], glob_id);
				roxml_release_id(root, node_set, *count, req_ids[path_id + 1]);
			}
		}
	}
	roxml_release_id(root, node_set, *count, glob_id);

	for (path_id = 0; path_id < index; path_id++) {
		if (req_ids[path_id] != 0) {
			roxml_release_id(root, node_set, *count, req_ids[path_id]);
		}
	}

	free(req_ids);

	return node_set;
}

ROXML_INT int _func_xpath_ignore(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return 1;
}

ROXML_INT int _func_xpath_new_node(char *chunk, void *data)
{
	int cur = 0;
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	if (!ctx->quoted && !ctx->dquoted && !ctx->parenthesys && !ctx->bracket) {
		int offset = 0;
		xpath_node_t *tmp_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
		if ((chunk[cur] == '/') && (ctx->is_first_node)) {
			free(tmp_node);
			ctx->new_node = ctx->first_node;
			ctx->first_node->abs = 1;
		} else if ((chunk[cur] == '/') && (ctx->wait_first_node)) {
			free(tmp_node);
			ctx->first_node->abs = 1;
		} else if ((ctx->is_first_node) || (ctx->wait_first_node)) {
			free(tmp_node);
		} else {
			if (ctx->new_node) {
				ctx->new_node->next = tmp_node;
			}
			ctx->new_node = tmp_node;
		}
		ctx->is_first_node = 0;
		ctx->wait_first_node = 0;
		ctx->new_node = roxml_set_axes(ctx->new_node, chunk + cur, &offset);
		cur = offset + 1;
	}
	ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_quote(char *chunk, void *data)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	if (!ctx->dquoted) {
		if (ctx->quoted && ctx->content_quoted == MODE_COMMENT_QUOTE) {
			ctx->content_quoted = MODE_COMMENT_NONE;
			chunk[0] = '\0';
		}
		ctx->quoted = (ctx->quoted + 1) % 2;
	}
	ctx->shorten_cond = 0;
	return 1;
}

ROXML_INT int _func_xpath_dquote(char *chunk, void *data)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	if (!ctx->quoted) {
		if (ctx->dquoted && ctx->content_quoted == MODE_COMMENT_DQUOTE) {
			ctx->content_quoted = MODE_COMMENT_NONE;
			chunk[0] = '\0';
		}
		ctx->dquoted = (ctx->dquoted + 1) % 2;
	}
	ctx->shorten_cond = 0;
	return 1;
}

ROXML_INT int _func_xpath_open_parenthesys(char *chunk, void *data)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	if (!ctx->quoted && !ctx->dquoted) {
		ctx->parenthesys = (ctx->parenthesys + 1) % 2;
	}
	ctx->shorten_cond = 0;
	return 1;
}

ROXML_INT int _func_xpath_close_parenthesys(char *chunk, void *data)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	if (!ctx->quoted && !ctx->dquoted) {
		ctx->parenthesys = (ctx->parenthesys + 1) % 2;
	}
	ctx->shorten_cond = 0;
	return 1;
}

ROXML_INT int _func_xpath_open_brackets(char *chunk, void *data)
{
	xpath_cond_t *tmp_cond;
	int cur = 0;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	if (!ctx->quoted && !ctx->dquoted) {
		ctx->bracket = (ctx->bracket + 1) % 2;
		chunk[0] = '\0';

		ctx->shorten_cond = 1;
		tmp_cond = (xpath_cond_t *) calloc(1, sizeof(xpath_cond_t));
		ctx->new_node->cond = tmp_cond;
		ctx->new_cond = tmp_cond;
		ctx->new_cond->arg1 = chunk + cur + 1;
	} else {
		ctx->shorten_cond = 0;
	}
	cur++;
	return 1;
}

ROXML_INT int _func_xpath_close_brackets(char *chunk, void *data)
{
	int cur = 0;
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	if (!ctx->quoted && !ctx->dquoted) {
		ctx->bracket = (ctx->bracket + 1) % 2;
		chunk[0] = '\0';

		if (ctx->new_cond) {
			if (ctx->new_cond->func == ROXML_FUNC_XPATH) {
				xpath_node_t *xp;
				ctx->new_cond->func2 = roxml_parse_xpath(ctx->new_cond->arg1, &xp, 1);
				ctx->new_cond->xp = xp;
			}
		} else {
			return -1;
		}
	}
	cur++;
	ctx->shorten_cond = 0;
	return 1;
}

ROXML_INT int _func_xpath_condition_or(char *chunk, void *data)
{
	xpath_node_t *tmp_node;
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	int len = 0;
	xpath_cond_t *tmp_cond;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */

	len = strlen(ROXML_COND_OR);

	if (strncmp(chunk, ROXML_COND_OR, len) == 0) {
		if (roxml_is_separator(*(chunk - 1)) && roxml_is_separator(*(chunk + len))) {
			if (!ctx->bracket && !ctx->quoted && !ctx->dquoted) {
				if (ctx->context != 1) {
					return 0;
				}
				chunk[-1] = '\0';
				cur += strlen(ROXML_COND_OR);
				tmp_node = (xpath_node_t *) calloc(ctx->nbpath + 1, sizeof(xpath_node_t));
				memcpy(tmp_node, ctx->first_node, ctx->nbpath * sizeof(xpath_node_t));
				free(ctx->first_node);
				ctx->first_node = tmp_node;
				ctx->wait_first_node = 1;
				ctx->new_node = tmp_node + ctx->nbpath;
				ctx->new_node->rel = ROXML_OPERATOR_OR;
				ctx->nbpath++;
			} else if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
				if (ctx->new_cond->func != ROXML_FUNC_XPATH) {
					chunk[-1] = '\0';
					cur += strlen(ROXML_COND_OR);
					tmp_cond = (xpath_cond_t *) calloc(1, sizeof(xpath_cond_t));
					if (ctx->new_cond) {
						ctx->new_cond->next = tmp_cond;
					}
					ctx->new_cond = tmp_cond;
					ctx->new_cond->rel = ROXML_OPERATOR_OR;
					ctx->new_cond->arg1 = chunk + cur + 1;
				}
			}
		}
	}
	if (cur)
		ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_condition_and(char *chunk, void *data)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	int len = 0;
	xpath_node_t *tmp_node;
	xpath_cond_t *tmp_cond;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */

	len = strlen(ROXML_COND_AND);

	if (strncmp(chunk, ROXML_COND_AND, len) == 0) {
		if (roxml_is_separator(*(chunk - 1)) && roxml_is_separator(*(chunk + len))) {
			if (!ctx->bracket && !ctx->quoted && !ctx->dquoted) {
				if (ctx->context != 1) {
					return 0;
				}
				chunk[-1] = '\0';
				cur += strlen(ROXML_COND_AND);
				tmp_node = (xpath_node_t *) calloc(ctx->nbpath + 1, sizeof(xpath_node_t));
				memcpy(tmp_node, ctx->first_node, ctx->nbpath * sizeof(xpath_node_t));
				free(ctx->first_node);
				ctx->first_node = tmp_node;
				ctx->wait_first_node = 1;
				ctx->new_node = tmp_node + ctx->nbpath;
				ctx->new_node->rel = ROXML_OPERATOR_AND;
				ctx->nbpath++;
			} else if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
				if (ctx->new_cond->func != ROXML_FUNC_XPATH) {
					chunk[-1] = '\0';
					cur += strlen(ROXML_COND_AND);
					tmp_cond = (xpath_cond_t *) calloc(1, sizeof(xpath_cond_t));
					if (ctx->new_cond) {
						ctx->new_cond->next = tmp_cond;
					}
					ctx->new_cond = tmp_cond;
					ctx->new_cond->rel = ROXML_OPERATOR_AND;
					ctx->new_cond->arg1 = chunk + cur + 1;
				}
			}
		}
	}
	if (cur)
		ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_path_or(char *chunk, void *data)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	xpath_node_t *tmp_node;
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */

	if (!ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		chunk[-1] = '\0';
		cur += strlen(ROXML_PATH_OR);
		tmp_node = (xpath_node_t *) calloc(ctx->nbpath + 1, sizeof(xpath_node_t));
		memcpy(tmp_node, ctx->first_node, ctx->nbpath * sizeof(xpath_node_t));
		free(ctx->first_node);
		ctx->first_node = tmp_node;
		ctx->wait_first_node = 1;
		ctx->new_node = tmp_node + ctx->nbpath;
		ctx->new_node->rel = ROXML_OPERATOR_OR;
		ctx->nbpath++;
	}
	ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_operators(char *chunk, void *data, int operator, int operator_bis)
{
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	if (!ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		xpath_node_t *xp_root = ctx->new_node;
		xpath_cond_t *xp_cond = (xpath_cond_t *) calloc(1, sizeof(xpath_cond_t));
		xp_root->xp_cond = xp_cond;
		chunk[cur] = '\0';
		xp_cond->op = operator;
		if (ROXML_WHITE(chunk[cur - 1])) {
			chunk[cur - 1] = '\0';
		}
		if (chunk[cur + 1] == '=') {
			cur++;
			chunk[cur] = '\0';
			xp_cond->op = operator_bis;
		}
		if (ROXML_WHITE(chunk[cur + 1])) {
			cur++;
			chunk[cur] = '\0';
		}
		xp_cond->arg2 = chunk + cur + 1;
		if (xp_cond->arg2[0] == '"') {
			ctx->content_quoted = MODE_COMMENT_DQUOTE;
			xp_cond->arg2++;
		} else if (xp_cond->arg2[0] == '\'') {
			ctx->content_quoted = MODE_COMMENT_QUOTE;
			xp_cond->arg2++;
		}
		if (!xp_cond->func) {
			xp_cond->func = ROXML_FUNC_INTCOMP;
			if (!roxml_is_number(xp_cond->arg2)) {
				xp_cond->func = ROXML_FUNC_STRCOMP;
			}
		}
		cur++;
	} else if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		if (ctx->new_cond->func != ROXML_FUNC_XPATH) {
			chunk[cur] = '\0';
			ctx->new_cond->op = operator;
			if (ROXML_WHITE(chunk[cur - 1])) {
				chunk[cur - 1] = '\0';
			}
			if (chunk[cur + 1] == '=') {
				cur++;
				chunk[cur] = '\0';
				ctx->new_cond->op = operator_bis;
			}
			if (ROXML_WHITE(chunk[cur + 1])) {
				cur++;
				chunk[cur] = '\0';
			}
			ctx->new_cond->arg2 = chunk + cur + 1;
			if (ctx->new_cond->arg2[0] == '"') {
				ctx->content_quoted = MODE_COMMENT_DQUOTE;
				ctx->new_cond->arg2++;
			} else if (ctx->new_cond->arg2[0] == '\'') {
				ctx->content_quoted = MODE_COMMENT_QUOTE;
				ctx->new_cond->arg2++;
			}
			if (ctx->new_cond->func == 0) {
				ctx->new_cond->func = ROXML_FUNC_INTCOMP;
				if (!roxml_is_number(ctx->new_cond->arg2)) {
					ctx->new_cond->func = ROXML_FUNC_STRCOMP;
				}
			}
			cur++;
		}
	}
	ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_operator_equal(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_operators(chunk, data, ROXML_OPERATOR_EQU, ROXML_OPERATOR_EQU);
}

ROXML_INT int _func_xpath_operator_sup(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_operators(chunk, data, ROXML_OPERATOR_SUP, ROXML_OPERATOR_ESUP);
}

ROXML_INT int _func_xpath_operator_inf(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_operators(chunk, data, ROXML_OPERATOR_INF, ROXML_OPERATOR_EINF);
}

ROXML_INT int _func_xpath_operator_diff(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_operators(chunk, data, ROXML_OPERATOR_DIFF, ROXML_OPERATOR_DIFF);
}

ROXML_INT int _func_xpath_number(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		if ((ctx->new_cond->func != ROXML_FUNC_XPATH) && (ctx->shorten_cond)) {
			cur = 1;
			ctx->new_cond->func = ROXML_FUNC_POS;
			ctx->new_cond->op = ROXML_OPERATOR_EQU;
			ctx->new_cond->arg2 = chunk;
			while ((chunk[cur + 1] >= '0') && (chunk[cur + 1] <= '9')) {
				cur++;
			}
		}
	}
	ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_funcs(char *chunk, void *data, int func, char *name)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;

	if (strncmp(chunk, name, strlen(name)) == 0) {
		if (ctx->new_cond->func != func) {
			cur += strlen(name);
			ctx->new_cond->func = func;
		}
	}
	if (cur)
		ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_position(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_funcs(chunk, data, ROXML_FUNC_POS, ROXML_FUNC_POS_STR);
}

ROXML_INT int _func_xpath_first(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_funcs(chunk, data, ROXML_FUNC_FIRST, ROXML_FUNC_FIRST_STR);
}

ROXML_INT int _func_xpath_last(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_funcs(chunk, data, ROXML_FUNC_LAST, ROXML_FUNC_LAST_STR);
}

ROXML_INT int _func_xpath_nsuri(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_funcs(chunk, data, ROXML_FUNC_NSURI, ROXML_FUNC_NSURI_STR);
}

ROXML_INT int _func_xpath_lname(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	return _func_xpath_funcs(chunk, data, ROXML_FUNC_LNAME, ROXML_FUNC_LNAME_STR);
}

ROXML_INT int _func_xpath_operator_add(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		if (ctx->new_cond->func != ROXML_FUNC_XPATH) {
			if ((ctx->new_cond->func == ROXML_FUNC_LAST) || (ctx->new_cond->func == ROXML_FUNC_FIRST)) {
				ctx->new_cond->op = ROXML_OPERATOR_ADD;
			}
			chunk[cur] = '\0';
			if (ROXML_WHITE(chunk[cur + 1])) {
				cur++;
				chunk[cur] = '\0';
			}
			ctx->new_cond->arg2 = chunk + cur + 1;
		}
	}
	ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_operator_subs(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;
	int cur = 0;
	if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		if (ctx->new_cond->func != ROXML_FUNC_XPATH) {
			if ((ctx->new_cond->func == ROXML_FUNC_LAST) || (ctx->new_cond->func == ROXML_FUNC_FIRST)) {
				ctx->new_cond->op = ROXML_OPERATOR_SUB;
			}
			chunk[cur] = '\0';
			if (ROXML_WHITE(chunk[cur + 1])) {
				cur++;
				chunk[cur] = '\0';
			}
			ctx->new_cond->arg2 = chunk + cur + 1;
		}
	}
	ctx->shorten_cond = 0;
	return cur;
}

ROXML_INT int _func_xpath_default(char *chunk, void *data)
{
#ifdef DEBUG_PARSING
	fprintf(stderr, "calling func %s chunk %c\n", __func__, chunk[0]);
#endif /* DEBUG_PARSING */
	int cur = 0;
	roxml_xpath_ctx_t *ctx = (roxml_xpath_ctx_t *) data;

	if ((ctx->is_first_node) || (ctx->wait_first_node)) {
		if (!ctx->quoted && !ctx->dquoted && !ctx->parenthesys && !ctx->bracket) {
			int offset = 0;
			xpath_node_t *tmp_node = (xpath_node_t *) calloc(1, sizeof(xpath_node_t));
			if ((chunk[cur] == '/') && (ctx->is_first_node)) {
				free(tmp_node);
				ctx->new_node = ctx->first_node;
				ctx->first_node->abs = 1;
			} else if ((chunk[cur] == '/') && (ctx->wait_first_node)) {
				free(tmp_node);
				ctx->first_node->abs = 1;
			} else if ((ctx->is_first_node) || (ctx->wait_first_node)) {
				free(tmp_node);
			} else {
				if (ctx->new_node) {
					ctx->new_node->next = tmp_node;
				}
				ctx->new_node = tmp_node;
			}
			ctx->is_first_node = 0;
			ctx->wait_first_node = 0;
			ctx->new_node = roxml_set_axes(ctx->new_node, chunk + cur, &offset);
			cur += offset;
		}
	} else if (ctx->bracket && !ctx->quoted && !ctx->dquoted) {
		if (ctx->new_cond->func != ROXML_FUNC_XPATH) {
			if (ctx->shorten_cond) {
				int bracket_lvl = 1;
				ctx->new_cond->func = ROXML_FUNC_XPATH;
				ctx->new_cond->arg1 = chunk + cur;
				while (bracket_lvl > 0) {
					if (chunk[cur] == '[') {
						bracket_lvl++;
					} else if (chunk[cur] == ']') {
						bracket_lvl--;
					}
					cur++;
				}
				cur--;
			}
		}
	}
	ctx->shorten_cond = 0;
	return cur > 0 ? cur : 1;
}