FROM jupyter/base-notebook

USER ${NB_UID}
COPY . /home/jovyan/prajna
RUN /home/jovyan/prajna/bin/prajna jupyter -i
USER root
RUN sudo ln -s /home/jovyan/prajna/bin/prajna /usr/local/bin/prajna
USER ${NB_UID}
