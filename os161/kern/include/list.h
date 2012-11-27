#define REMOVEHEAD(head, tail, node) \
{\
        if (tail == head) { \
                    node = head;\
                    head = NULL;\
                    tail = NULL;\
                } else {\
                            node = head;\
                            head = head->next;\
                            assert(head != NULL);\
                            assert(tail != NULL);\
                        }\
        if (node) node->next = NULL;\
}

#define ADDTOTAIL(head, tail, node) \
{\
        if (tail == NULL) {\
                    assert(head == NULL);\
                    tail = node; \
                    head = node;\
                } else {\
                            assert(head != NULL);\
                            tail->next = node;\
                            tail = node;\
                        }\
}
