FROM jupyter/base-notebook

USER ${NB_UID}
COPY . /home/jovyan/prajna
RUN /home/jovyan/prajna/bin/prajna jupyter -i
